/* vi: set sw=4 ts=4: */
/*
 * wget - retrieve a file using HTTP or FTP
 *
 * Chip Rosenthal Covad Communications <chip@laserlink.net>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

#ifdef CONFIG_FEATURE_WGET_HTTPS 
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif
#ifdef SUPPORT_GPL_1
#include "cms_util.h"
#include "cms_msg.h"
#endif
struct host_info {
	// May be used if we ever will want to free() all xstrdup()s...
	/* char *allocated; */
	const char *path;
	const char *user;
	char       *host;
	int         port;
	smallint    is_ftp;
#ifdef CONFIG_FEATURE_WGET_HTTPS
	smallint    is_https;
#endif   
};


/* Globals (can be accessed from signal handlers) */
struct globals {
	off_t content_len;        /* Content-length of the file */
	off_t beg_range;          /* Range at which continue begins */
#if ENABLE_FEATURE_WGET_STATUSBAR
	off_t transferred;        /* Number of bytes transferred so far */
	const char *curfile;      /* Name of current file being transferred */
	bb_progress_t pmt;
#endif
	smallint chunked;         /* chunked transfer encoding */
	smallint got_clen;        /* got content-length: from server  */
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
struct BUG_G_too_big {
	char BUG_G_too_big[sizeof(G) <= COMMON_BUFSIZE ? 1 : -1];
};
#define INIT_G() do { } while (0)

#ifdef CONFIG_FEATURE_WGET_HTTPS
/*--------------------*/
typedef struct HttpHdrs{
  /* common */
  char *content_type;
  char *protocol;
  char *wwwAuthenticate;
  char *Authorization;
  char *TransferEncoding;
  char *Connection;
  /* request */
  char *method;
  char *path;
  char *host;
  int  port;
  int  content_length;

  /* result */
  int  status_code;
  char *message;
  char *locationHdr;		/* from 3xx status response */

  /* request derived */
  unsigned int addr;  /* IP-address of communicating entity */
  char *filename;
  char *arg;
} HttpHdrs;

static HttpHdrs httphdrs;

static void ssl_Init(void);
static int ssl_send_get_request(SSL *ssl, const char *host, const char *uri, char *authhdr);
static int ssl_rev_header_resp(SSL *ssl, int fd, HttpHdrs *hdrs, char *errmsg);
static int ssl_rev_file_date(SSL *ssl, HttpHdrs *hdrs, int stream, char *errmsg);

static SSL_CTX *ssl_ctx = NULL;
static char HostStr[]="Host:";
static char ConnectionStr[]="Connection:";
static char ContentLthStr[]="Content-Length:";
static char ContentTypeStr[]="Content-Type:";
static char WWWAuthenticateStr[]="WWW-Authenticate:";
static char AuthorizationStr[]="Authorization:";
static char TransferEncoding[]="Transfer-Encoding:";
static char LocationStr[]="Location:";

#define BUF_SIZE_MAX 4096

//#define WGET_HTTPS_DEBUG
#ifdef WGET_HTTPS_DEBUG
#define DEBUGP(args...) fprintf(stdout, args)
#else
#define DEBUGP(args...)  
#endif

#ifdef USE_CERTIFICATES
#define WGET_CIPHERS     "RSA:DES:SHA+RSA:RC4:SAH+MEDIUM"
#else
#define WGET_CIPHERS     "SSLv3"
#endif
#endif /*CONFIG_FEATURE_WGET_HTTPS*/

#if ENABLE_FEATURE_WGET_STATUSBAR

static void progress_meter(int flag)
{
	/* We can be called from signal handler */
	int save_errno = errno;

	if (flag == -1) { /* first call to progress_meter */
		bb_progress_init(&G.pmt);
	}

	bb_progress_update(&G.pmt, G.curfile, G.beg_range, G.transferred,
			   G.chunked ? 0 : G.beg_range + G.transferred + G.content_len);

	if (flag == 0) {
		/* last call to progress_meter */
		alarm(0);
		bb_putchar_stderr('\n');
		G.transferred = 0;
	} else {
		if (flag == -1) { /* first call to progress_meter */
			signal_SA_RESTART_empty_mask(SIGALRM, progress_meter);
		}
		alarm(1);
	}

	errno = save_errno;
}

#else /* FEATURE_WGET_STATUSBAR */

static ALWAYS_INLINE void progress_meter(int flag UNUSED_PARAM) { }

#endif


/* IPv6 knows scoped address types i.e. link and site local addresses. Link
 * local addresses can have a scope identifier to specify the
 * interface/link an address is valid on (e.g. fe80::1%eth0). This scope
 * identifier is only valid on a single node.
 *
 * RFC 4007 says that the scope identifier MUST NOT be sent across the wire,
 * unless all nodes agree on the semantic. Apache e.g. regards zone identifiers
 * in the Host header as invalid requests, see
 * https://issues.apache.org/bugzilla/show_bug.cgi?id=35122
 */
static void strip_ipv6_scope_id(char *host)
{
	char *scope, *cp;

	/* bbox wget actually handles IPv6 addresses without [], like
	 * wget "http://::1/xxx", but this is not standard.
	 * To save code, _here_ we do not support it. */

	if (host[0] != '[')
		return; /* not IPv6 */

	scope = strchr(host, '%');
	if (!scope)
		return;

	/* Remove the IPv6 zone identifier from the host address */
	cp = strchr(host, ']');
	if (!cp || (cp[1] != ':' && cp[1] != '\0')) {
		/* malformed address (not "[xx]:nn" or "[xx]") */
		return;
	}

	/* cp points to "]...", scope points to "%eth0]..." */
	overlapping_strcpy(scope, cp);
}

/* Read NMEMB bytes into PTR from STREAM.  Returns the number of bytes read,
 * and a short count if an eof or non-interrupt error is encountered.  */
static size_t safe_fread(void *ptr, size_t nmemb, FILE *stream)
{
	size_t ret;
	char *p = (char*)ptr;

	do {
		clearerr(stream);
		errno = 0;
		ret = fread(p, 1, nmemb, stream);
		p += ret;
		nmemb -= ret;
	} while (nmemb && ferror(stream) && errno == EINTR);

	return p - (char*)ptr;
}

/* Read a line or SIZE-1 bytes into S, whichever is less, from STREAM.
 * Returns S, or NULL if an eof or non-interrupt error is encountered.  */
static char *safe_fgets(char *s, int size, FILE *stream)
{
	char *ret;

	do {
		clearerr(stream);
		errno = 0;
		ret = fgets(s, size, stream);
	} while (ret == NULL && ferror(stream) && errno == EINTR);

	return ret;
}

#if ENABLE_FEATURE_WGET_AUTHENTICATION
/* Base64-encode character string. buf is assumed to be char buf[512]. */
static char *base64enc_512(char buf[512], const char *str)
{
	unsigned len = strlen(str);
	if (len > 512/4*3 - 10) /* paranoia */
		len = 512/4*3 - 10;
	bb_uuencode(buf, str, len, bb_uuenc_tbl_base64);
	return buf;
}
#endif

static char* sanitize_string(char *s)
{
	unsigned char *p = (void *) s;
	while (*p >= ' ')
		p++;
	*p = '\0';
	return s;
}

static FILE *open_socket(len_and_sockaddr *lsa)
{
	FILE *fp;

	/* glibc 2.4 seems to try seeking on it - ??! */
	/* hopefully it understands what ESPIPE means... */
	fp = fdopen(xconnect_stream(lsa), "r+");
	if (fp == NULL)
		bb_perror_msg_and_die("fdopen");

	return fp;
}

static int ftpcmd(const char *s1, const char *s2, FILE *fp, char *buf)
{
	int result;
	if (s1) {
		if (!s2) s2 = "";
		fprintf(fp, "%s%s\r\n", s1, s2);
		fflush(fp);
	}

	do {
		char *buf_ptr;

		if (fgets(buf, 510, fp) == NULL) {
			bb_perror_msg_and_die("error getting response");
		}
		buf_ptr = strstr(buf, "\r\n");
		if (buf_ptr) {
			*buf_ptr = '\0';
		}
	} while (!isdigit(buf[0]) || buf[3] != ' ');

	buf[3] = '\0';
	result = xatoi_u(buf);
	buf[3] = ' ';
	return result;
}

static void parse_url(char *src_url, struct host_info *h)
{
	char *url, *p, *sp;

	/* h->allocated = */ url = xstrdup(src_url);

	if (strncmp(url, "http://", 7) == 0) {
		h->port = bb_lookup_port("http", "tcp", 80);
		h->host = url + 7;
		h->is_ftp = 0;
      #ifdef CONFIG_FEATURE_WGET_HTTPS
          h->is_https = 0;
      #endif
	} else if (strncmp(url, "ftp://", 6) == 0) {
		h->port = bb_lookup_port("ftp", "tcp", 21);
		h->host = url + 6;
		h->is_ftp = 1;
   #ifdef CONFIG_FEATURE_WGET_HTTPS
          h->is_https = 0;
	}else if (strncmp(url, "https://", 8) == 0){
          h->port = bb_lookup_port("https", "tcp", 443);
		h->host = url + 8;
		h->is_ftp = 0;
          h->is_https = 1;
   #endif
	} else
		bb_error_msg_and_die("not an http or ftp url: %s", sanitize_string(url));

	// FYI:
	// "Real" wget 'http://busybox.net?var=a/b' sends this request:
	//   'GET /?var=a/b HTTP 1.0'
	//   and saves 'index.html?var=a%2Fb' (we save 'b')
	// wget 'http://busybox.net?login=john@doe':
	//   request: 'GET /?login=john@doe HTTP/1.0'
	//   saves: 'index.html?login=john@doe' (we save '?login=john@doe')
	// wget 'http://busybox.net#test/test':
	//   request: 'GET / HTTP/1.0'
	//   saves: 'index.html' (we save 'test')
	//
	// We also don't add unique .N suffix if file exists...
	sp = strchr(h->host, '/');
	p = strchr(h->host, '?'); if (!sp || (p && sp > p)) sp = p;
	p = strchr(h->host, '#'); if (!sp || (p && sp > p)) sp = p;
	if (!sp) {
		h->path = "";
	} else if (*sp == '/') {
		*sp = '\0';
		h->path = sp + 1;
	} else { // '#' or '?'
		// http://busybox.net?login=john@doe is a valid URL
		// memmove converts to:
		// http:/busybox.nett?login=john@doe...
		memmove(h->host - 1, h->host, sp - h->host);
		h->host--;
		sp[-1] = '\0';
		h->path = sp;
	}

	// We used to set h->user to NULL here, but this interferes
	// with handling of code 302 ("object was moved")

	sp = strrchr(h->host, '@');
	if (sp != NULL) {
		h->user = h->host;
		*sp = '\0';
		h->host = sp + 1;
	}

	sp = h->host;
}

static char *gethdr(char *buf, size_t bufsiz, FILE *fp /*, int *istrunc*/)
{
	char *s, *hdrval;
	int c;

	/* *istrunc = 0; */

	/* retrieve header line */
	if (fgets(buf, bufsiz, fp) == NULL)
		return NULL;

	/* see if we are at the end of the headers */
	for (s = buf; *s == '\r'; ++s)
		continue;
	if (*s == '\n')
		return NULL;

	/* convert the header name to lower case */
	for (s = buf; isalnum(*s) || *s == '-' || *s == '.'; ++s) {
		/* tolower for "A-Z", no-op for "0-9a-z-." */
		*s = (*s | 0x20);
	}

	/* verify we are at the end of the header name */
	if (*s != ':')
		bb_error_msg_and_die("bad header line: %s", sanitize_string(buf));

	/* locate the start of the header value */
	*s++ = '\0';
	hdrval = skip_whitespace(s);

	/* locate the end of header */
	while (*s && *s != '\r' && *s != '\n')
		++s;

	/* end of header found */
	if (*s) {
		*s = '\0';
		return hdrval;
	}

	/* Rats! The buffer isn't big enough to hold the entire header value */
	while (c = getc(fp), c != EOF && c != '\n')
		continue;
	/* *istrunc = 1; */
	return hdrval;
}

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
static char *URL_escape(const char *str)
{
	/* URL encode, see RFC 2396 */
	char *dst;
	char *res = dst = xmalloc(strlen(str) * 3 + 1);
	unsigned char c;

	while (1) {
		c = *str++;
		if (c == '\0'
		/* || strchr("!&'()*-.=_~", c) - more code */
		 || c == '!'
		 || c == '&'
		 || c == '\''
		 || c == '('
		 || c == ')'
		 || c == '*'
		 || c == '-'
		 || c == '.'
		 || c == '='
		 || c == '_'
		 || c == '~'
		 || (c >= '0' && c <= '9')
		 || ((c|0x20) >= 'a' && (c|0x20) <= 'z')
		) {
			*dst++ = c;
			if (c == '\0')
				return res;
		} else {
			*dst++ = '%';
			*dst++ = bb_hexdigits_upcase[c >> 4];
			*dst++ = bb_hexdigits_upcase[c & 0xf];
		}
	}
}
#endif

static FILE* prepare_ftp_session(FILE **dfpp, struct host_info *target, len_and_sockaddr *lsa)
{
	char buf[512];
	FILE *sfp;
	char *str;
	int port;

	if (!target->user)
		target->user = xstrdup("anonymous:busybox@");

	sfp = open_socket(lsa);
	if (ftpcmd(NULL, NULL, sfp, buf) != 220)
		bb_error_msg_and_die("%s", sanitize_string(buf+4));

	/*
	 * Splitting username:password pair,
	 * trying to log in
	 */
	str = strchr(target->user, ':');
	if (str)
		*str++ = '\0';
	switch (ftpcmd("USER ", target->user, sfp, buf)) {
	case 230:
		break;
	case 331:
		if (ftpcmd("PASS ", str, sfp, buf) == 230)
			break;
		/* fall through (failed login) */
	default:
		bb_error_msg_and_die("ftp login: %s", sanitize_string(buf+4));
	}

	ftpcmd("TYPE I", NULL, sfp, buf);

	/*
	 * Querying file size
	 */
	if (ftpcmd("SIZE ", target->path, sfp, buf) == 213) {
		G.content_len = BB_STRTOOFF(buf+4, NULL, 10);
		if (G.content_len < 0 || errno) {
			bb_error_msg_and_die("SIZE value is garbage");
		}
		G.got_clen = 1;
	}

	/*
	 * Entering passive mode
	 */
	if (ftpcmd("PASV", NULL, sfp, buf) != 227) {
 pasv_error:
		bb_error_msg_and_die("bad response to %s: %s", "PASV", sanitize_string(buf));
	}
	// Response is "227 garbageN1,N2,N3,N4,P1,P2[)garbage]
	// Server's IP is N1.N2.N3.N4 (we ignore it)
	// Server's port for data connection is P1*256+P2
	str = strrchr(buf, ')');
	if (str) str[0] = '\0';
	str = strrchr(buf, ',');
	if (!str) goto pasv_error;
	port = xatou_range(str+1, 0, 255);
	*str = '\0';
	str = strrchr(buf, ',');
	if (!str) goto pasv_error;
	port += xatou_range(str+1, 0, 255) * 256;
	set_nport(lsa, htons(port));

	*dfpp = open_socket(lsa);

	if (G.beg_range) {
		sprintf(buf, "REST %"OFF_FMT"u", G.beg_range);
		if (ftpcmd(buf, NULL, sfp, buf) == 350)
			G.content_len -= G.beg_range;
	}

	if (ftpcmd("RETR ", target->path, sfp, buf) > 150)
		bb_error_msg_and_die("bad response to %s: %s", "RETR", sanitize_string(buf));

	return sfp;
}

/* Must match option string! */
enum {
	WGET_OPT_CONTINUE   = (1 << 0),
	WGET_OPT_SPIDER	    = (1 << 1),
	WGET_OPT_QUIET      = (1 << 2),
	WGET_OPT_OUTNAME    = (1 << 3),
	WGET_OPT_PREFIX     = (1 << 4),
	WGET_OPT_PROXY      = (1 << 5),
	WGET_OPT_USER_AGENT = (1 << 6),
	WGET_OPT_RETRIES    = (1 << 7),
	WGET_OPT_NETWORK_READ_TIMEOUT = (1 << 8),
	WGET_OPT_PASSIVE    = (1 << 9),
	WGET_OPT_HEADER     = (1 << 10) * ENABLE_FEATURE_WGET_LONG_OPTIONS,
	WGET_OPT_POST_DATA  = (1 << 11) * ENABLE_FEATURE_WGET_LONG_OPTIONS,
#ifdef SUPPORT_GPL_1
        WGET_OPT_FLASH_IMG  = (1 << 12),
#if defined(AEI_FACTORY_TEST)
        WGET_OPT_NO_REBOOT  = (1 << 13), 
#endif
#endif
};
#ifdef SUPPORT_GPL_1
// use some globals to reduce the messiness of change the original tftp function.
char *glbImgPtr = NULL;
int glbUpldSize = 0;
//int brcm_tftp = 0;
char *glbCurrentPtr = NULL;
char glbUpldType = 'i';
static void *msgHandle=NULL;
#define TFTP_BLKSIZE_DEFAULT       512
#define BLOCK_ALLOC 1000000
/* let's do a 1MB alloc and add realloc increment upto max flash size */
static int newWrite(char *inBuf, int inBufLen)
{
   static SINT32 allocSize = 0;
   char *myglbCurPtr = NULL;

   if (glbImgPtr == NULL)
   {
      if (inBufLen < TFTP_BLKSIZE_DEFAULT)   // not enough data for a valid first packet and exit
         return -1;

      // Allocate maximum flash image size + possible broadcom header TAG.
      // (Don't bother getting the length from the broadcom TAG, we don't
      // get a TAG if it is a whole image anyways.)
      // The Linux kernel will not assign physical pages to the buffer
      // until we write to it, so it is OK if we allocate a little more
      // than we really need.

      allocSize = BLOCK_ALLOC ; //cmsImg_getImageFlashSize() + cmsImg_getBroadcomImageTagSize();
      printf("Allocating %d bytes for flash image.\n", allocSize);
      if ((glbImgPtr = (char *) malloc(allocSize)) == NULL)
      {
         printf("Not enough memory error.  Could not allocate %u bytes.", allocSize);
         return -1;
      }
      else
      {
         printf("Memory allocated %u bytes\n",allocSize);
         glbCurrentPtr = glbImgPtr;
         if (glbUpldSize + inBufLen < allocSize)
         {
            memcpy(glbImgPtr, inBuf, inBufLen);
            glbCurrentPtr += inBufLen;
            glbUpldSize += inBufLen;
            return inBufLen;
         }
      }
   }
   else
   {
      if (glbUpldSize + inBufLen < allocSize)
      {
          memcpy(glbCurrentPtr, inBuf, inBufLen);
          glbCurrentPtr += inBufLen;
          glbUpldSize += inBufLen;
      }
      else
      {
          /* need to reset pointers */
          char *myglbCurPtr = NULL;
          SINT32 maxSize = cmsImg_getImageFlashSize() + cmsImg_getBroadcomImageTagSize();
          allocSize = glbUpldSize + BLOCK_ALLOC;
          if ( allocSize > maxSize )
          {
              allocSize = maxSize;
          }

          myglbCurPtr  = realloc(glbImgPtr, allocSize);

          if (myglbCurPtr)
          {
              printf("Memory reallocated %u bytes\n",allocSize);
              glbImgPtr = myglbCurPtr;
              glbCurrentPtr = myglbCurPtr +  glbUpldSize;
              if (glbUpldSize + inBufLen < allocSize)
              {
                  memcpy(glbCurrentPtr, inBuf, inBufLen);
                  glbCurrentPtr += inBufLen;
                  glbUpldSize += inBufLen;
              }
              else
              {
                  printf("Image could not fit into %u byte buffer.\n", allocSize);
                  return -1;
              }
          }
          else
          {
              printf("Image could not fit into %u byte buffer.\n", allocSize);
              return -1;

          }
      }
   }

   return inBufLen;
}
#endif

static void NOINLINE retrieve_file_data(FILE *dfp, int output_fd)
{
	char buf[512];

	if (!(option_mask32 & WGET_OPT_QUIET))
		progress_meter(-1);

	if (G.chunked)
		goto get_clen;

	/* Loops only if chunked */
	while (1) {
		while (1) {
			int n;
			unsigned rdsz;

			rdsz = sizeof(buf);
			if (G.got_clen) {
				if (G.content_len < (off_t)sizeof(buf)) {
					if ((int)G.content_len <= 0)
						break;
					rdsz = (unsigned)G.content_len;
				}
			}
			n = safe_fread(buf, rdsz, dfp);
			if (n <= 0) {
				if (ferror(dfp)) {
					/* perror will not work: ferror doesn't set errno */
					bb_error_msg_and_die(bb_msg_read_error);
				}
				break;
			}
#ifdef SUPPORT_GPL_1
                        newWrite(buf, n);
#else
			xwrite(output_fd, buf, n);
#endif
#if ENABLE_FEATURE_WGET_STATUSBAR
			G.transferred += n;
#endif
			if (G.got_clen)
				G.content_len -= n;
		}

		if (!G.chunked)
			break;

		safe_fgets(buf, sizeof(buf), dfp); /* This is a newline */
 get_clen:
		safe_fgets(buf, sizeof(buf), dfp);
		G.content_len = STRTOOFF(buf, NULL, 16);
		/* FIXME: error check? */
		if (G.content_len == 0)
			break; /* all done! */
		G.got_clen = 1;
	}

	if (!(option_mask32 & WGET_OPT_QUIET))
		progress_meter(0);
}
#if defined(AEI_FACTORY_TEST)
extern int forceNoReboot;
#endif

int wget_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int wget_main(int argc UNUSED_PARAM, char **argv)
{
	char buf[512];
	struct host_info server, target;
	len_and_sockaddr *lsa;
	unsigned opt;
	int redir_limit;
	char *proxy = NULL;
	char *dir_prefix = NULL;
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	char *post_data;
	char *extra_headers = NULL;
	llist_t *headers_llist = NULL;
#endif
	FILE *sfp;                      /* socket to web/ftp server         */
	FILE *dfp;                      /* socket to ftp server (data)      */
	char *fname_out;                /* where to direct output (-O)      */
	int output_fd = -1;
	bool use_proxy;                 /* Use proxies if env vars are set  */
	const char *proxy_flag = "on";  /* Use proxies if env vars are set  */
	const char *user_agent = "Wget";/* "User-Agent" header field        */

	static const char keywords[] ALIGN1 =
		"content-length\0""transfer-encoding\0""chunked\0""location\0";
	enum {
		KEY_content_length = 1, KEY_transfer_encoding, KEY_chunked, KEY_location
	};
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	static const char wget_longopts[] ALIGN1 =
		/* name, has_arg, val */
		"continue\0"         No_argument       "c"
		"spider\0"           No_argument       "s"
		"quiet\0"            No_argument       "q"
		"output-document\0"  Required_argument "O"
		"directory-prefix\0" Required_argument "P"
		"proxy\0"            Required_argument "Y"
		"user-agent\0"       Required_argument "U"
		/* Ignored: */
		// "tries\0"            Required_argument "t"
		// "timeout\0"          Required_argument "T"
		/* Ignored (we always use PASV): */
		"passive-ftp\0"      No_argument       "\xff"
		"header\0"           Required_argument "\xfe"
		"post-data\0"        Required_argument "\xfd"
		/* Ignored (we don't do ssl) */
		"no-check-certificate\0" No_argument   "\xfc"
		;
#endif
#ifdef SUPPORT_GPL_1
     CmsRet ret;
     CmsLogLevel logLevel=DEFAULT_LOG_LEVEL;
#endif
	INIT_G();
#ifdef SUPPORT_GPL_1
    cmsLog_init(EID_WGET);
    cmsLog_setLevel(logLevel);
#endif
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	applet_long_options = wget_longopts;
#endif
	/* server.allocated = target.allocated = NULL; */
	opt_complementary = "-1" IF_FEATURE_WGET_LONG_OPTIONS(":\xfe::");
	opt = getopt32(argv, "csqO:P:Y:U:" /*ignored:*/ "t:T:"
#ifdef SUPPORT_GPL_1
                   /*pdl is not used, it is just fit the bit placeholder*/
                               "pdlf"
#endif
#if defined(AEI_FACTORY_TEST)
   "n"
#endif
                                ,
				&fname_out, &dir_prefix,
				&proxy_flag, &user_agent,
				NULL, /* -t RETRIES */
				NULL /* -T NETWORK_READ_TIMEOUT */
				IF_FEATURE_WGET_LONG_OPTIONS(, &headers_llist)
				IF_FEATURE_WGET_LONG_OPTIONS(, &post_data)
				);
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	if (headers_llist) {
		int size = 1;
		char *cp;
		llist_t *ll = headers_llist;
		while (ll) {
			size += strlen(ll->data) + 2;
			ll = ll->link;
		}
		extra_headers = cp = xmalloc(size);
		while (headers_llist) {
			cp += sprintf(cp, "%s\r\n", (char*)llist_pop(&headers_llist));
		}
	}
#endif
#ifdef SUPPORT_GPL_1
         /* We need to establish a comm link with smd. */
       if ((ret = cmsMsg_init(EID_WGET, &msgHandle)) != CMSRET_SUCCESS)
        {
           bb_error_msg("failed to open comm link with smd, WGET failed.");
           return 0;
        }
#endif

	/* TODO: compat issue: should handle "wget URL1 URL2..." */

	target.user = NULL;
	parse_url(argv[optind], &target);

	/* Use the proxy if necessary */
	use_proxy = (strcmp(proxy_flag, "off") != 0);
#ifdef CONFIG_FEATURE_WGET_HTTPS
     //don't support proxy for https now
     if(target.is_https)
     {
          use_proxy = 0;
     }
#endif
	if (use_proxy) {
		proxy = getenv(target.is_ftp ? "ftp_proxy" : "http_proxy");
		if (proxy && proxy[0]) {
			server.user = NULL;
			parse_url(proxy, &server);
		} else {
			use_proxy = 0;
		}
	}
	if (!use_proxy) {
		server.port = target.port;
		if (ENABLE_FEATURE_IPV6) {
			server.host = xstrdup(target.host);
		} else {
			server.host = target.host;
		}
	}

	if (ENABLE_FEATURE_IPV6)
		strip_ipv6_scope_id(target.host);

	/* Guess an output filename, if there was no -O FILE */
	if (!(opt & WGET_OPT_OUTNAME)) {
		fname_out = bb_get_last_path_component_nostrip(target.path);
		/* handle "wget http://kernel.org//" */
		if (fname_out[0] == '/' || !fname_out[0])
			fname_out = (char*)"index.html";
		/* -P DIR is considered only if there was no -O FILE */
		if (dir_prefix)
			fname_out = concat_path_file(dir_prefix, fname_out);
	} else {
		if (LONE_DASH(fname_out)) {
			/* -O - */
			output_fd = 1;
			opt &= ~WGET_OPT_CONTINUE;
		}
	}
#if ENABLE_FEATURE_WGET_STATUSBAR
	G.curfile = bb_get_last_path_component_nostrip(fname_out);
#endif

	/* Impossible?
	if ((opt & WGET_OPT_CONTINUE) && !fname_out)
		bb_error_msg_and_die("can't specify continue (-c) without a filename (-O)");
	*/

	/* Determine where to start transfer */
	if (opt & WGET_OPT_CONTINUE) {
		output_fd = open(fname_out, O_WRONLY);
		if (output_fd >= 0) {
			G.beg_range = xlseek(output_fd, 0, SEEK_END);
		}
		/* File doesn't exist. We do not create file here yet.
		 * We are not sure it exists on remove side */
	}

	redir_limit = 5;
 resolve_lsa:
	lsa = xhost2sockaddr(server.host, server.port);
	if (!(opt & WGET_OPT_QUIET)) {
		char *s = xmalloc_sockaddr2dotted(&lsa->u.sa);
		fprintf(stderr, "Connecting to %s (%s)\n", server.host, s);
		free(s);
	}
 establish_session:
#ifdef CONFIG_FEATURE_WGET_HTTPS
   if(!target.is_https){
#endif   
	if (use_proxy || !target.is_ftp) {
		/*
		 *  HTTP session
		 */
		char *str;
		int status;

		/* Open socket to http server */
		sfp = open_socket(lsa);

		/* Send HTTP request */
		if (use_proxy) {
			fprintf(sfp, "GET %stp://%s/%s HTTP/1.1\r\n",
				target.is_ftp ? "f" : "ht", target.host,
				target.path);
		} else {
			if (opt & WGET_OPT_POST_DATA)
				fprintf(sfp, "POST /%s HTTP/1.1\r\n", target.path);
			else
				fprintf(sfp, "GET /%s HTTP/1.1\r\n", target.path);
		}

		fprintf(sfp, "Host: %s\r\nUser-Agent: %s\r\n",
			target.host, user_agent);

#if ENABLE_FEATURE_WGET_AUTHENTICATION
		if (target.user) {
			fprintf(sfp, "Proxy-Authorization: Basic %s\r\n"+6,
				base64enc_512(buf, target.user));
		}
		if (use_proxy && server.user) {
			fprintf(sfp, "Proxy-Authorization: Basic %s\r\n",
				base64enc_512(buf, server.user));
		}
#endif

		if (G.beg_range)
			fprintf(sfp, "Range: bytes=%"OFF_FMT"u-\r\n", G.beg_range);
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
		if (extra_headers)
			fputs(extra_headers, sfp);

		if (opt & WGET_OPT_POST_DATA) {
			char *estr = URL_escape(post_data);
			fprintf(sfp, "Content-Type: application/x-www-form-urlencoded\r\n");
			fprintf(sfp, "Content-Length: %u\r\n" "\r\n" "%s",
					(int) strlen(estr), estr);
			/*fprintf(sfp, "Connection: Keep-Alive\r\n\r\n");*/
			/*fprintf(sfp, "%s\r\n", estr);*/
			free(estr);
		} else
#endif
		{ /* If "Connection:" is needed, document why */
			fprintf(sfp, /* "Connection: close\r\n" */ "\r\n");
		}

		/*
		 * Retrieve HTTP response line and check for "200" status code.
		 */
 read_response:
		if (fgets(buf, sizeof(buf), sfp) == NULL)
			bb_error_msg_and_die("no response from server");

		str = buf;
		str = skip_non_whitespace(str);
		str = skip_whitespace(str);
		// FIXME: no error check
		// xatou wouldn't work: "200 OK"
		status = atoi(str);
		switch (status) {
		case 0:
		case 100:
			while (gethdr(buf, sizeof(buf), sfp /*, &n*/) != NULL)
				/* eat all remaining headers */;
			goto read_response;
		case 200:
/*
Response 204 doesn't say "null file", it says "metadata
has changed but data didn't":

"10.2.5 204 No Content
The server has fulfilled the request but does not need to return
an entity-body, and might want to return updated metainformation.
The response MAY include new or updated metainformation in the form
of entity-headers, which if present SHOULD be associated with
the requested variant.

If the client is a user agent, it SHOULD NOT change its document
view from that which caused the request to be sent. This response
is primarily intended to allow input for actions to take place
without causing a change to the user agent's active document view,
although any new or updated metainformation SHOULD be applied
to the document currently in the user agent's active view.

The 204 response MUST NOT include a message-body, and thus
is always terminated by the first empty line after the header fields."

However, in real world it was observed that some web servers
(e.g. Boa/0.94.14rc21) simply use code 204 when file size is zero.
*/
		case 204:
			break;
		case 300:	/* redirection */
		case 301:
		case 302:
		case 303:
			break;
		case 206:
			if (G.beg_range)
				break;
			/* fall through */
		default:
			bb_error_msg_and_die("server returned error: %s", sanitize_string(buf));
		}

		/*
		 * Retrieve HTTP headers.
		 */
		while ((str = gethdr(buf, sizeof(buf), sfp /*, &n*/)) != NULL) {
			/* gethdr converted "FOO:" string to lowercase */
			smalluint key;
			/* strip trailing whitespace */
			char *s = strchrnul(str, '\0') - 1;
			while (s >= str && (*s == ' ' || *s == '\t')) {
				*s = '\0';
				s--;
			}
			key = index_in_strings(keywords, buf) + 1;
			if (key == KEY_content_length) {
				G.content_len = BB_STRTOOFF(str, NULL, 10);
				if (G.content_len < 0 || errno) {
					bb_error_msg_and_die("content-length %s is garbage", sanitize_string(str));
				}
				G.got_clen = 1;
				continue;
			}
			if (key == KEY_transfer_encoding) {
				if (index_in_strings(keywords, str_tolower(str)) + 1 != KEY_chunked)
					bb_error_msg_and_die("transfer encoding '%s' is not supported", sanitize_string(str));
				G.chunked = G.got_clen = 1;
			}
			if (key == KEY_location && status >= 300) {
				if (--redir_limit == 0)
					bb_error_msg_and_die("too many redirections");
				fclose(sfp);
				G.got_clen = 0;
				G.chunked = 0;
				if (str[0] == '/')
					/* free(target.allocated); */
					target.path = /* target.allocated = */ xstrdup(str+1);
					/* lsa stays the same: it's on the same server */
				else {
					parse_url(str, &target);
					if (!use_proxy) {
						server.host = target.host;
						/* strip_ipv6_scope_id(target.host); - no! */
						/* we assume remote never gives us IPv6 addr with scope id */
						server.port = target.port;
						free(lsa);
						goto resolve_lsa;
					} /* else: lsa stays the same: we use proxy */
				}
				goto establish_session;
			}
		}
//		if (status >= 300)
//			bb_error_msg_and_die("bad redirection (no Location: header from server)");

		/* For HTTP, data is pumped over the same connection */
		dfp = sfp;

	} else {
		/*
		 *  FTP session
		 */
		sfp = prepare_ftp_session(&dfp, &target, lsa);
	}

	if (opt & WGET_OPT_SPIDER) {
		if (ENABLE_FEATURE_CLEAN_UP)
			fclose(sfp);
		return EXIT_SUCCESS;
	}

	if (output_fd < 0) {
		int o_flags = O_WRONLY | O_CREAT | O_TRUNC | O_EXCL;
		/* compat with wget: -O FILE can overwrite */
		if (opt & WGET_OPT_OUTNAME)
			o_flags = O_WRONLY | O_CREAT | O_TRUNC;
		output_fd = xopen(fname_out, o_flags);
	}

	retrieve_file_data(dfp, output_fd);
	xclose(output_fd);

	if (dfp != sfp) {
		/* It's ftp. Close it properly */
		fclose(dfp);
		if (ftpcmd(NULL, NULL, sfp, buf) != 226)
			bb_error_msg_and_die("ftp error: %s", sanitize_string(buf+4));
		/* ftpcmd("QUIT", NULL, sfp, buf); - why bother? */
	}
#ifdef CONFIG_FEATURE_WGET_HTTPS
   }/*!target.is_https*/
   else //support WGET_HTTPS
   {
      int ssl_fd, i=0;
      SSL *ssl;
      int sslConn;
      char errmsg[256];
      HttpHdrs *hdrs = &httphdrs;
#ifdef CONFIG_FEATURE_WGET_AUTHENTICATION
      char authhdr[256];
#endif
      
      ssl_Init();
      if((ssl_fd = xconnect_stream(lsa)) <0)
         bb_error_msg_and_die("Create socket to remote host (%s) fail.",
				inet_ntoa(((struct sockaddr_in *)(&lsa->u.sa))->sin_addr));

      ssl = SSL_new(ssl_ctx);
      if(ssl == NULL)
         bb_error_msg_and_die("SSL failed: SSL_new.");

      if(!SSL_set_fd (ssl, ssl_fd))
         bb_error_msg_and_die("SSL failed: SSL_set_fd.");

      SSL_set_connect_state (ssl);
      if (SSL_connect (ssl) <= 0 || ssl->state != SSL_ST_OK)
         bb_error_msg_and_die("SSL failed: SSL_connect.");
      
      memset(hdrs, 0, sizeof(struct HttpHdrs));
      //send request header
#ifdef CONFIG_FEATURE_WGET_AUTHENTICATION
      if (target.user) {
      	i = sprintf(authhdr, "Authorization: Basic %s\r\n", base64enc_512(buf, target.user));
          authhdr[i] = '\0';

          if(ssl_send_get_request(ssl, target.host, target.path, authhdr) <0)
            bb_error_msg_and_die("SSL failed: Send Get Request Error.");
      }
      else if(ssl_send_get_request(ssl, target.host, target.path, NULL) <0)
               bb_error_msg_and_die("SSL failed: Send Get Request Error.");
#else
      if(ssl_send_get_request(ssl, target.host, target.path, NULL) <0)
         bb_error_msg_and_die("SSL failed: Send Get Request Error.");
#endif
      
      //get header response and parse it
      if(ssl_rev_header_resp(ssl, ssl_fd, hdrs, errmsg) < 0)
         bb_error_msg_and_die(errmsg);

      if(hdrs->status_code < 100 || hdrs->status_code >= 600)
         bb_error_msg_and_die("SSL failed: status code %d from serve is error.", hdrs->status_code);

      if (hdrs->status_code == 200 && 
         ((hdrs->content_length > 0) ||
          (hdrs->TransferEncoding && !strcasecmp(hdrs->TransferEncoding,"chunked"))))
      {
         int mlth = 0;
         char *rambuf = NULL;
         
         DEBUGP("Starting download file");
         
         if (output_fd < 0) {
		   int o_flags = O_WRONLY | O_CREAT | O_TRUNC | O_EXCL;
		   /* compat with wget: -O FILE can overwrite */
		   if (opt & WGET_OPT_OUTNAME)
			   o_flags = O_WRONLY | O_CREAT | O_TRUNC;
		   output_fd = xopen(fname_out, o_flags);
	    }  
         
         //retrieve file data
         if(ssl_rev_file_date(ssl, hdrs, output_fd, errmsg) < 0){
            xclose(output_fd);
		  unlink(fname_out);
            bb_error_msg_and_die(errmsg);           
         }
      }
      else if (hdrs->status_code == 401)
      {
         bb_error_msg_and_die("SSL failed: 401 Unauthorized.");
      }
      else if (hdrs->status_code == 404)
      {
         bb_error_msg_and_die("SSL failed: 404 NOT FOUND.");
      }
      else
      {
         bb_error_msg_and_die("SSL failed: No support status code %d.", hdrs->status_code);
      }      
   }
#endif
   fprintf(stdout, "200 OK, File Get Success\n");
#ifdef SUPPORT_GPL_1
       if (opt & WGET_OPT_FLASH_IMG)
    {
#if defined(AEI_FACTORY_TEST)
       if (opt & WGET_OPT_NO_REBOOT)
          forceNoReboot = 1;
#endif
       fprintf(stdout, "began to flashing image\n");
       if ((ret = cmsImg_writeImage(glbImgPtr, glbUpldSize, msgHandle)) != CMSRET_SUCCESS)
       {
          bb_error_msg("Wget Image failed: Illegal image.\n");
       }
       /*If we get here, Tell smd to go back to normal operation.*/
#if defined(AEI_FACTORY_TEST)
       if (opt & WGET_OPT_NO_REBOOT)
          forceNoReboot = 0;
#endif

       cmsImg_sendLoadDoneMsg(msgHandle);
    }

    if (glbImgPtr)
    {
       free(glbImgPtr);
    }

    /*for security issue, forbid wget anyfile to the DUT*/
    unlink(fname_out);
#endif

	return EXIT_SUCCESS;
}

#ifdef CONFIG_FEATURE_WGET_HTTPS
/*======================================================================*
 * ssl_Init
 *======================================================================*/
void ssl_Init()
{
   DEBUGP("Enter ssl_init()\n");
   
   SSL_load_error_strings();
   SSL_library_init();
   ssl_ctx = SSL_CTX_new(SSLv3_client_method());
   if (ssl_ctx == NULL) 
   {
      bb_error_msg_and_die("Could not create SSL context");
   }

   if (! SSL_CTX_set_cipher_list(ssl_ctx, WGET_CIPHERS) )
   {
      DEBUGP("Err: Could not set cipher list for SSL\n");
   }

#ifdef USE_CERTIFICATES
   {
      struct stat fstat;
      // PT: add to support client certificate
      if (lstat(CLIENT_CERT_FILE, &fstat) == 0) 
      {
         if (lstat(CLIENT_PRIVATE_KEY_FILE, &fstat) == 0)
         {         
            if (SSL_CTX_use_certificate_file(ssl_ctx, CLIENT_CERT_FILE, SSL_FILETYPE_PEM) <= 0) 
            {
               DEBUGP("Err: Error loading the client certificate\n");
            }
            if (SSL_CTX_use_PrivateKey_file(ssl_ctx, CLIENT_PRIVATE_KEY_FILE, SSL_FILETYPE_PEM) <= 0) 
            {
               DEBUGP("Err: Error loading the client private key\n");
            }
            if (! SSL_CTX_check_private_key(ssl_ctx)) 
            {
               DEBUGP("Err: Private key does not match the client certificate public key\n");
            }
         } 
         else 
         {
            DEBUGP("Err: No private key found\n");
         }
      } 
      else 
      {
         DEBUGP("Notice: No client certificate found\n");
      }
      
      if (lstat(ROOT_CERT_FILE, &fstat) == 0) 
      {
         int retval = SSL_CTX_load_verify_locations(ssl_ctx, ROOT_CERT_FILE, CERT_PATH);
         /* for both ssl, retval == 1 is load verified */
         if (retval != 1)
         {
            DEBUGP("Err: Could not load verify locations\n");
         }
         /* if fail to load certificate, set the certificate verify anyway */		      
         SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, verify_callback);
      } 
      else 
      {
         /* for bcm ssl, if no certificate in the system, just skip the certificate check */
         DEBUGP("Notice: No server certificate found. Skip checking on certificate.\n");
         SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, 0);
      }
   }   
#endif // USE_CERTIFICATES

   SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
   ERR_print_errors_fp(stderr);
   SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_OFF);
}  /* End of proto_Init() */

/*----------------------------------------------------------------------
 * removes any trailing whitespaces, \r and \n
 * it destroys its argument...
 */
void www_StripTail(char *s)
{
  if (*s != '\0') {
    while(*s) s++;
    s--;
    while(*s == '\r' || *s == '\n' || *s == ' ' || *s == '\t') {
      *s = '\0';
      s--;
    }
  }
}

int read_timeout(int socket_, int timeOutSec_)
{
   fd_set readSet;

   FD_ZERO(&readSet);
   FD_SET(socket_, &readSet);
   if (timeOutSec_ == 0)
   {
      // zero means BLOCKING operation (will wait indefinitely)
      return (select(socket_ + 1, &readSet, NULL, NULL, NULL));
   }
    // otherwise, wait up to the specified time period
    struct timeval tv;

    tv.tv_sec = timeOutSec_;
    tv.tv_usec = 0;

    return (select(socket_ + 1, &readSet, NULL, NULL, &tv));

    // returns 0 if the time limit expired.
    // returns -1 on error, otherwise there is data on the port ready to read
}

int ssl_Readn(SSL *ssl, char *ptr, int nbytes)
{
    int fd, nleft, nread=0;
    int   errnoval;

    nleft = nbytes;
    while (nleft > 0) {
      errno =0;
      if((fd = SSL_get_fd(ssl)) <0)
         return -1;
      
      if (read_timeout(fd, 30) <= 0) 
      {
        return -99; //timeout!!!
      }  
      
      nread = SSL_read(ssl, (void *) ptr, nleft);        

      if (nread < 0) {                            /* This function will read until the byte cnt*/
         errnoval=errno;                         /* is reached or the return is <0. In the case*/
         if (errnoval==EAGAIN )                  /* of non-blocking reads this may happen after*/
             return nbytes-nleft;                /* some bytes have been retrieved. The EAGAIN*/
         else                                    /* status indicates that more are coming */
            return nread; /* error, return < 0 */
      } else if (nread == 0) {
         break; /* EOF */
      }

      nleft -= nread;
      ptr += nread;
   }

   return nbytes - nleft; /* return >= 0 */
}

int ssl_Readline(SSL *ssl, char *buf, int maxlen)
{
    int fd, n, rc;
    char   *ptr = buf;
    char c;
    int flags, bflags;

    if((fd = SSL_get_fd(ssl)) <0)
         return -1;
    
    for (n = 1; n < maxlen; n++) {
        rc = ssl_Readn(ssl, &c, 1);
        if (rc == 1) {
            *ptr++ = c;
            if (c == '\n')
                break;
        } else if (rc == 0) {
            if (n == 1) {
                return 0; /* EOF, no data read */
            } else
                break;    /* EOF, some data was read */
        } else {
            return -1; /* ERROR */
        }
    }

    *ptr = '\0';

    DEBUGP("ssl_Readline : %s", buf);
    return n;
}

static char *ssl_readLengthMsg(SSL *ssl, int readLth, int *mlth, int doFlushStream, char *errmsg) {
   int bufCnt = 0, readCnt = 0, nread=0;
   int bufLth = readLth;
   char *soapBuf = NULL;
   char c;

   *mlth = 0;
   /*
    * This is the path taken when we do image download.  Don't zeroize
    * the buffer that is allocated here because that will force linux
    * to immediately assign physical pages to the buffer.  Intead, just
    * let the buffer fill in as the transfer progresses.  This will give
    * smd and the kernel more time to make physical pages available.
    */
   if ((soapBuf = (char *) malloc(readLth + 1)) != NULL)
   {
      while (bufCnt < readLth)
      {
         // if ((readCnt = proto_ReadWait(wg->pc, soapBuf+bufCnt, bufLth)) > 0)
         if ((readCnt = ssl_Readn(ssl, soapBuf+bufCnt, bufLth)) > 0)
         {
            bufCnt += readCnt;
            bufLth -= readCnt;
         }
         else
         {
            if (readCnt == -99) {
                  /* read error */
                  if(soapBuf != NULL)
                     free(soapBuf);
                  soapBuf = NULL;
                  strcpy(errmsg, "SSL failed: read socket timeout.\n");
                  break;
               }
            }
      }
      DEBUGP("soapBuf bufCnt=%d readLth=%d\n", bufCnt, readLth);
      if(readCnt != -99)
      {
         *mlth = bufCnt;
         soapBuf[bufCnt] = '\0';
      }
      if (doFlushStream)
      {
         do {
               nread = SSL_read(ssl, &c, 1);
         } while (nread>0);   
      }
   }
   else
      strcpy(errmsg, "SSL failed: malloc failed in ssl_readLengthMsg.\n");
      
   return soapBuf;
}

static char *ssl_readChunkedMsg(SSL *ssl, int *mlth, int maxSize, char *errmsg) {
   char *soapBuf = NULL;
   char chunkedBuf[128];   
   char c;
   int nread=0;
   
   *mlth = 0;
   // read chunked size of first chunk*/
   if (ssl_Readline(ssl, chunkedBuf, sizeof(chunkedBuf)) > 0)
   {
      int  chunkedSz = 0, readSz = 0;
      char *newBuf = NULL, *readBuf = NULL;
      
      sscanf(chunkedBuf, "%x", &chunkedSz);
      while (chunkedSz > 0)
      {
         // read chunked data
         int doFlushStream=FALSE;
         readBuf = ssl_readLengthMsg(ssl, chunkedSz, &readSz, doFlushStream, errmsg);
         if (chunkedSz != readSz)
            DEBUGP("===> ssl_readChunkedMsg, chunked size = %d, read size = %d\n", chunkedSz, readSz);
         if (readBuf == NULL)
         {
            if(soapBuf)
               free(soapBuf);
            *mlth = 0;
            break;
         }
         if ((*mlth + readSz) > maxSize)
         {
            sprintf(errmsg, "SSL failed: reading more data than maxSize (%d)\n", maxSize);
            if(soapBuf)
               free(soapBuf);
            *mlth = 0;
            if(readBuf)
               free(readBuf);
            readSz = 0;
            break;
         }
         if (soapBuf == NULL)
         {
            // allocate the first chunk since cmsMem_realloc
            // does not accept soapBuf as NULL pointer.
            newBuf = soapBuf = malloc(*mlth + readSz);
         }
         else 
         {
            // reallocate soap message size
            newBuf = realloc(soapBuf, *mlth + readSz);
         }
         
         if (newBuf == NULL)
         {
            strcpy(errmsg, "SSL failed: malloc failed in ssl_readChunkedMsg\n");

            if(soapBuf)
               free(soapBuf);
            *mlth = 0;
            if(readBuf)
               free(readBuf);
            readSz = 0;
            break;
         }
         // point soap message to new allocated memory
         soapBuf = newBuf;
         // append chunked data to soap message 
         strncpy(soapBuf + *mlth, readBuf, readSz);
         // increase soap message size
         *mlth += readSz;
         // free chunked data
         if(readBuf)
            free(readBuf);
         readSz = 0;
         chunkedSz = 0;
         // flush off trailing crlf
         do
         {
            chunkedBuf[0] = '\0';
            readSz = ssl_Readline(ssl, chunkedBuf, sizeof(chunkedBuf));
         } while (readSz > 0 && isxdigit(chunkedBuf[0]) == 0);
         // read chunked size of next chunk*/
         if (isxdigit(chunkedBuf[0]) != 0)
         {
            sscanf(chunkedBuf, "%x", &chunkedSz);
         }
         else
         {
            if(soapBuf)
               free(soapBuf);
            *mlth = 0;
         }
      }      
      // skip(flush) anything else
      do {
            nread = SSL_read(ssl, &c, 1);
      } while (nread>0);   
   }
   else
      strcpy(errmsg, "SSL failed: read chunked size of first chunk.\n");

   return soapBuf;
}

int ssl_send_get_request(SSL *ssl, const char *host, const char *uri, char *authhdr)
{
   char buf[BUF_SIZE_MAX];
   int len=0, nwritten=0;

   if(uri==NULL || host==NULL)
      return -1;
   
   len = sprintf(buf,"GET /%s HTTP/1.1\r\n", uri);
   len += sprintf(buf+len,"Host: %s\r\nUser-Agent: Wget\r\n", host);

   if(authhdr != NULL)
      len += sprintf(buf+len,authhdr);
   
   len += sprintf(buf+len,"Connection: close\r\n\r\n");
   
   nwritten = SSL_write(ssl, buf, len);

   buf[len] = '\0';
   DEBUGP("Send Request header to server: [%s] len = %d, nwritten = %d\n", buf, len, nwritten);

   if(nwritten != len)
      return -1;
   else
      return nwritten;
}  /* End of ssl_send_get_request() */

int ssl_rev_header_resp(SSL *ssl, int fd, HttpHdrs *hdrs, char *errmsg)
{
    char buf[BUF_SIZE_MAX];
    char protocol[BUF_SIZE_MAX];
    char status[BUF_SIZE_MAX];
    char message[BUF_SIZE_MAX];
    char *cp;
    int status_code, n;

    DEBUGP("Enter proto_ParseResponse()");    

    /*step 1: Parse the first line of the request. */
    if (ssl_Readline(ssl, buf, BUF_SIZE_MAX) <= 0) {
        strcpy(errmsg, "SSL failed: read the first line of header error");
        return -1;
    }

    if (sscanf(buf, "%[^ ] %[^ ] %[^\r]", protocol, status, message ) != 3) {
        strcpy(errmsg, "SSL failed: can't get protocol/status/message in resp header.");
        return -1;
    }

    www_StripTail(protocol);
    www_StripTail(status);
    www_StripTail(message);
    if(hdrs->protocol != NULL)
      free(hdrs->protocol);
    hdrs->protocol = strdup(protocol);
    hdrs->status_code = atoi(status); /* TBD: add sanity check */
    if(hdrs->message != NULL)
      free(hdrs->message);
    hdrs->message = strdup(message);
    DEBUGP("proto_ParseResponse, step 1: (protocol=\"%s\", status=%d message=\"%s\")",
            hdrs->protocol, hdrs->status_code, hdrs->message);

    /*step 2: Parse the rest of the request headers. */
    while ((n = ssl_Readline(ssl, buf, BUF_SIZE_MAX)) > 0) 
    {
        www_StripTail(buf);
        DEBUGP("proto_ParseResponse, step 2:  read \"%s\"", buf);
        
        if (strcmp(buf, "") == 0) {
            break;
        } else if (strncasecmp(buf, HostStr,sizeof(HostStr)-1) == 0) {
            cp = &buf[sizeof(HostStr)-1];
            cp += strspn(cp, " \t");
            if(hdrs->host != NULL)
               free(hdrs->host);
            hdrs->host = strdup(cp);
        } else if (strncasecmp(buf, ContentLthStr,sizeof(ContentLthStr)-1) == 0) {
            cp = &buf[sizeof(ContentLthStr)-1];
            cp += strspn(cp, " \t");
            hdrs->content_length = atoi(cp);
        } else if (strncasecmp(buf, ContentTypeStr,sizeof(ContentTypeStr)-1) == 0) {
            cp = &buf[sizeof(ContentTypeStr)-1];
            cp += strspn(cp, " \t");
            if(hdrs->content_type != NULL)
               free(hdrs->content_type);
            hdrs->content_type = strdup(cp);
        } else if (strncasecmp(buf, ConnectionStr,sizeof(ConnectionStr)-1) == 0) {
            cp = &buf[sizeof(ConnectionStr)-1];
            cp += strspn(cp, " \t");
            if(hdrs->Connection != NULL)
               free(hdrs->Connection);
            hdrs->Connection = strdup(cp);
        } else if (strncasecmp(buf, WWWAuthenticateStr, sizeof(WWWAuthenticateStr)-1)==0) {
            cp =&buf[sizeof(WWWAuthenticateStr)-1];
            cp += strspn(cp, " \t");
            if(hdrs->wwwAuthenticate != NULL)
               free(hdrs->wwwAuthenticate);
            hdrs->wwwAuthenticate = strdup(cp);
        } else if (strncasecmp(buf, AuthorizationStr, sizeof(AuthorizationStr)-1)==0) {
            cp =&buf[sizeof(AuthorizationStr)-1];
            cp += strspn(cp, " \t");
            if(hdrs->Authorization != NULL)
               free(hdrs->Authorization);
            hdrs->Authorization = strdup(cp);
        } else if (strncasecmp(buf, TransferEncoding, sizeof(TransferEncoding)-1)==0) {
            cp =&buf[sizeof(TransferEncoding)-1];
            cp += strspn(cp, " \t");
            if(hdrs->TransferEncoding != NULL)
               free(hdrs->TransferEncoding);
            hdrs->TransferEncoding = strdup(cp);
        } else if (strncasecmp(buf, LocationStr, sizeof(LocationStr)-1)==0) {
            cp =&buf[sizeof(LocationStr)-1];
            cp += strspn(cp, " \t");
            if(hdrs->locationHdr != NULL)
               free(hdrs->locationHdr);
            hdrs->locationHdr = strdup(cp);
        }
    }

    if(n < 0)
      strcpy(errmsg, "SSL failed: read rest of request header error.");
    
    return n;
}

int ssl_rev_file_date(SSL *ssl, HttpHdrs *hdrs, int stream, char *errmsg)
{
   char *soapBuf = NULL;
   int mlth = 0;

   if (hdrs->content_length > 0)
   {
      int doFlushStream = TRUE;

      /* this is the path taken by image downloads */
      DEBUGP("Get file data with content_length=%d", hdrs->content_length);
      soapBuf = ssl_readLengthMsg(ssl, hdrs->content_length, &mlth, doFlushStream, errmsg);
   }
   else if (hdrs->TransferEncoding && !strcasecmp(hdrs->TransferEncoding,"chunked"))
   {
      int maxSize = 1024*1024;
      DEBUGP("Get file data Chunked with maxSize=%d", maxSize);
      soapBuf = ssl_readChunkedMsg(ssl, &mlth, maxSize, errmsg);
   }

   //MUST free soapBuf if it's not NULL.
   if(soapBuf){
#ifdef SUPPORT_GPL_1
      newWrite(soapBuf, mlth);
#else
      xwrite(stream, soapBuf, mlth);
#endif
      free(soapBuf);
      return 0; 
   } 
   else
      return -1;
}
#endif
