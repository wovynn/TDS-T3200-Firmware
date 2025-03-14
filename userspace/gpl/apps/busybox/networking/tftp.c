/* vi: set sw=4 ts=4: */
/*
 * A simple tftp client/server for busybox.
 * Tries to follow RFC1350.
 * Only "octet" mode supported.
 * Optional blocksize negotiation (RFC2347 + RFC2348)
 *
 * Copyright (C) 2001 Magnus Damm <damm@opensource.se>
 *
 * Parts of the code based on:
 *
 * atftp:  Copyright (C) 2000 Jean-Pierre Lefebvre <helix@step.polymtl.ca>
 *                        and Remi Lefebvre <remi@debian.org>
 *
 * utftp:  Copyright (C) 1999 Uwe Ohse <uwe@ohse.de>
 *
 * tftpd added by Denys Vlasenko & Vladimir Dronnikov
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include "inet_common.h"
#include <net/if.h>

// brcm begin
#include "cms_util.h"
#include "cms_msg.h"

#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
#include "cms_image.h"

CmsImageTransferStats imageTransferStats;
#endif /* SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE */
// brcm end


#if ENABLE_FEATURE_TFTP_GET || ENABLE_FEATURE_TFTP_PUT

#define TFTP_BLKSIZE_DEFAULT       512  /* according to RFC 1350, don't change */
#define TFTP_BLKSIZE_DEFAULT_STR "512"
/* Was 50 ms but users asked to bump it up a bit */
#define TFTP_TIMEOUT_MS            100
#define TFTP_MAXTIMEOUT_MS        2000
#define TFTP_NUM_RETRIES            12  /* number of backed-off retries */

/* opcodes we support */
#define TFTP_RRQ   1
#define TFTP_WRQ   2
#define TFTP_DATA  3
#define TFTP_ACK   4
#define TFTP_ERROR 5
#define TFTP_OACK  6

/* error codes sent over network (we use only 0, 1, 3 and 8) */
/* generic (error message is included in the packet) */
#define ERR_UNSPEC   0
#define ERR_NOFILE   1
#define ERR_ACCESS   2
/* disk full or allocation exceeded */
#define ERR_WRITE    3
#define ERR_OP       4
#define ERR_BAD_ID   5
#define ERR_EXIST    6
#define ERR_BAD_USER 7
#define ERR_BAD_OPT  8

#if defined(SUPPORT_GPL_1)
#if defined(SUPPORT_GPL_2)
#define TFTPC_OPT_MAX_BASE 8    
#else
#define TFTPC_OPT_MAX_BASE 7
#endif
#if defined(AEI_DEFAULT_CFG_CUSTOMER)
#define TFTPC_OPT_MAX   (TFTPC_OPT_MAX_BASE+1)
#else
#define TFTPC_OPT_MAX   TFTPC_OPT_MAX_BASE
#endif
#endif
/* masks coming from getopt32 */
enum {
	TFTP_OPT_GET = (1 << 0),
	TFTP_OPT_PUT = (1 << 1),
#if defined AEI_DEFAULT_CFG_CUSTOMER
	TFTP_OPT_CUST_CFG = (1 << 2),   //follow CMD_GET(opt)
#endif
	/* pseudo option: if set, it's tftpd */
#if defined(SUPPORT_GPL_1)
	TFTPD_OPT = (1 << TFTPC_OPT_MAX) * ENABLE_TFTPD,
	TFTPD_OPT_r = (1 << TFTPC_OPT_MAX+1) * ENABLE_TFTPD,
	TFTPD_OPT_c = (1 << TFTPC_OPT_MAX+2) * ENABLE_TFTPD,
	TFTPD_OPT_u = (1 << TFTPC_OPT_MAX+3) * ENABLE_TFTPD,
#else
	TFTPD_OPT = (1 << 7) * ENABLE_TFTPD,
	TFTPD_OPT_r = (1 << 8) * ENABLE_TFTPD,
	TFTPD_OPT_c = (1 << 9) * ENABLE_TFTPD,
	TFTPD_OPT_u = (1 << 10) * ENABLE_TFTPD,
#endif
};

#if ENABLE_FEATURE_TFTP_GET && !ENABLE_FEATURE_TFTP_PUT
    #if defined AEI_DEFAULT_CFG_CUSTOMER
    #define CMD_CUST_CFG(cmd) ((cmd) & TFTP_OPT_CUST_CFG)
    #define IF_GETPUT(...) __VA_ARGS__
    #else
    #define CMD_CUST_CFG(cmd) 0
    #define IF_GETPUT(...)
    #endif
#define CMD_GET(cmd) 1
#define CMD_PUT(cmd) 0
#elif !ENABLE_FEATURE_TFTP_GET && ENABLE_FEATURE_TFTP_PUT
    #if defined AEI_DEFAULT_CFG_CUSTOMER
    #define CMD_CUST_CFG(cmd) ((cmd) & TFTP_OPT_CUST_CFG)
    #define IF_GETPUT(...) __VA_ARGS__
    #else
    #define CMD_CUST_CFG(cmd) 0
    #define IF_GETPUT(...)
    #endif
#define CMD_GET(cmd) 0
#define CMD_PUT(cmd) 1
#else
    #if defined AEI_DEFAULT_CFG_CUSTOMER
    #define CMD_CUST_CFG(cmd) ((cmd) & TFTP_OPT_CUST_CFG)
    #else
    #define CMD_CUST_CFG(cmd) 0
    #endif
#define IF_GETPUT(...) __VA_ARGS__
#define CMD_GET(cmd) ((cmd) & TFTP_OPT_GET)
#define CMD_PUT(cmd) ((cmd) & TFTP_OPT_PUT)
#endif
/* NB: in the code below
 * CMD_GET(cmd) and CMD_PUT(cmd) are mutually exclusive
 */


struct globals {
	/* u16 TFTP_ERROR; u16 reason; both network-endian, then error text: */
	uint8_t error_pkt[4 + 32];
	char *user_opt;
	/* used in tftpd_main(), a bit big for stack: */
	char block_buf[TFTP_BLKSIZE_DEFAULT];
#if ENABLE_FEATURE_TFTP_PROGRESS_BAR
	off_t pos;
	off_t size;
	const char *file;
	bb_progress_t pmt;
#endif
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
struct BUG_G_too_big {
	char BUG_G_too_big[sizeof(G) <= COMMON_BUFSIZE ? 1 : -1];
};
#define block_buf        (G.block_buf   )
#define user_opt         (G.user_opt    )
#define error_pkt        (G.error_pkt   )
#define INIT_G() do { } while (0)

#define error_pkt_reason (error_pkt[3])
#define error_pkt_str    (error_pkt + 4)

// brcm begin
// use some globals to reduce the messiness of change the original tftp function.
char *glbImagePtr = NULL;
int glbUploadSize = 0;
int brcm_tftp = 0;
char *glbCurPtr = NULL;
char glbUploadType = 'i';
static void *msgHandle=NULL;
/* 
 * connIfName is the linux interface name that our socket is going out of.
 * We need this information when doing image downloads because we might
 * want to kill all unneeded WAN services to free up memory.
 */
char connIfName[CMS_IFNAME_LENGTH]={0};


static int myRead(char *outBuf, int inLen)
{
   int readLen = 0;
   static int xmlCfgLen = 0;
   static int offset = 0;
   static CmsMsgHeader *responseMsg=NULL;
   CmsMsgHeader requestMsg = EMPTY_MSG_HEADER;
   char *cfgStart;
   CmsRet ret;
   
   
   if (responseMsg == NULL) 
   {
      cmsLog_debug("first time, get config file from smd");
      /*
       * This is the first time that we were called.
       * Send a message to smd to request a copy of the config file.
       */
      requestMsg.src = EID_TFTP;
      requestMsg.dst = EID_SMD;
      requestMsg.type = CMS_MSG_GET_CONFIG_FILE;
      requestMsg.flags_request = 1;
      
      if ((ret = cmsMsg_send(msgHandle, &requestMsg)) != CMSRET_SUCCESS)
      {
         cmsLog_error("could not send GET_CONFIG_FILE msg to smd.");
         return -1;
      }

      if ((ret = cmsMsg_receive(msgHandle, &responseMsg)) != CMSRET_SUCCESS)
      {
         cmsLog_error("could not receive GET_CONFIG_FILE msg from smd.");
         CMSMEM_FREE_BUF_AND_NULL_PTR(responseMsg);
         return -1;
      }      

      xmlCfgLen = (int) responseMsg->dataLength;
      cmsLog_debug("got config buffer len=%u", xmlCfgLen);
   }

   /* config data starts immediately after the header */
   cfgStart = (char *) (responseMsg + 1);
   
   if (xmlCfgLen <= inLen)
      readLen = xmlCfgLen;
   else
      readLen = inLen;

   memcpy(outBuf, (cfgStart + offset), readLen);

   xmlCfgLen -= readLen;
   offset += readLen;
   glbUploadSize += readLen;

   if (xmlCfgLen == 0)
   {
      /* done copying all the config data out, free the message */
      CMSMEM_FREE_BUF_AND_NULL_PTR(responseMsg);
      offset = 0;
      cmsLog_debug("send out entire config buf, free msg");
   }

   return readLen;
}


#if defined(SUPPORT_GPL_1)
static void tftp_killAppWhileUpgrade()
{
      system("killall -9 dhcpd");
      system("killall -9 sntp");
      system("killall -9 tr64c");
      system("killall -9 mynetwork");
      system("killall -9 rtd");
      system("killall -9 wlevt");
      system("killall -9 wlmngr");
#ifndef SUPPORT_GPL_2
      system("killall -9 consoled");
      system("killall -9 syslogd");
      system("killall -9 klogd");
#endif
      system("killall -9 mcpd");
      system("killall -9 urlfilterd");
      system("killall -9 detectWANService");
      system("killall -9 dsldiagd");
      system("killall -9 eapd");
      system("killall -9 nas");
      system("killall -9 lld2d");
      system("killall -9 wps_monitor");
      system("killall -9 acsd");
      system("killall -9 upnp");
      system("killall -9 ntpclient");
      system("killall -9 swmdk");
      system("killall -9 data_center");
      system("killall -9 pmd");
}
#endif

#if defined(SUPPORT_GPL_1)
#define BLOCK_ALLOC 1000000
/* let's do a 1MB alloc and add realloc increment upto max flash size */
static int myWrite(char *inBuf, int inBufLen)
{
   static SINT32 allocSize = 0;
   char *myglbCurPtr = NULL;

   if (glbImagePtr == NULL)
   {
      if (inBufLen < TFTP_BLKSIZE_DEFAULT)   // not enough data for a valid first packet and exit
         return -1;

      // Allocate maximum flash image size + possible broadcom header TAG.
      // (Don't bother getting the length from the broadcom TAG, we don't
      // get a TAG if it is a whole image anyways.)
      // The Linux kernel will not assign physical pages to the buffer
      // until we write to it, so it is OK if we allocate a little more
      // than we really need.

#if !defined(CHIP_63138)
      /*63138 has enough sdram*/
      tftp_killAppWhileUpgrade();
#endif

      allocSize = BLOCK_ALLOC ; //cmsImg_getImageFlashSize() + cmsImg_getBroadcomImageTagSize();
      printf("Allocating %d bytes for flash image.\n", allocSize);
      if ((glbImagePtr = (char *) malloc(allocSize)) == NULL)
      {
         printf("Not enough memory error.  Could not allocate %u bytes.", allocSize);
         return -1;
      }
      else
      {
         printf("Memory allocated %u bytes\n",allocSize);
         glbCurPtr = glbImagePtr;
         if (glbUploadSize + inBufLen < allocSize)
         {
            memcpy(glbImagePtr, inBuf, inBufLen);
            glbCurPtr += inBufLen;
            glbUploadSize += inBufLen;
            return inBufLen;
         }
      }
   }
   else
   {
      if (glbUploadSize + inBufLen < allocSize)
      {
          memcpy(glbCurPtr, inBuf, inBufLen);
          glbCurPtr += inBufLen;
          glbUploadSize += inBufLen;
      }
      else
      {
          /* need to reset pointers */
          char *myglbCurPtr = NULL;
          SINT32 maxSize = cmsImg_getImageFlashSize() + cmsImg_getBroadcomImageTagSize();
          allocSize = glbUploadSize + BLOCK_ALLOC;
          if ( allocSize > maxSize )
          {
              allocSize = maxSize;
          }

          myglbCurPtr  = realloc(glbImagePtr, allocSize);

          if (myglbCurPtr)
          {
              printf("Memory reallocated %u bytes\n",allocSize);
              glbImagePtr = myglbCurPtr;
              glbCurPtr = myglbCurPtr +  glbUploadSize;
              if (glbUploadSize + inBufLen < allocSize)
              {
                  memcpy(glbCurPtr, inBuf, inBufLen);
                  glbCurPtr += inBufLen;
                  glbUploadSize += inBufLen;
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
#else
static int myWrite(char *inBuf, int inBufLen)
{
   /* To use the old logic - allocate memory with flash size    
   * replace  CMS_IMAGE_MAX_ALLOC_LEN with 0, ie. 
   * int bcmImageGussstimated = 0;
   */
   SINT32 bcmImageGussstimated = CMS_IMAGE_MAX_ALLOC_LEN;
   static SINT32 allocSize = 0;

   if (glbCurPtr == NULL) 
   {
      UINT32 bcmImageSize = 0;
      
      if (inBufLen < TFTP_BLKSIZE_DEFAULT)   // not enough data for a valid first packet and exit
         return -1;   



      if (cmsImg_isBcmTaggedImage(inBuf, &bcmImageSize))
      {
         /* If it is a bcmTagged image, use the image length info from the bcmTag 
         * and also make bcmImageGussstimated 0 since bcmTagged image will not use that
         */
         allocSize =  bcmImageSize;
         bcmImageGussstimated = 0;
      }
      else
      {
         /* Not bcmTagged image.  Need to check if bcmImageGussstimated is initialized (> 0) or not */
         if (bcmImageGussstimated > 0)
         {
            /* If bcmImageGstmated size > 0, Use guestimated image size */
            allocSize = bcmImageGussstimated;
         }
         else
         {
            /* original logic if bcmImageGussstimated == 0, allocate memory with flash size plus bcm image tag */
            allocSize = cmsImg_getImageFlashSize() + cmsImg_getBroadcomImageTagSize();
         }     
      }

      bb_error_msg("Allocating %d bytes for flash image.\n", allocSize);
      
      if ((glbCurPtr = (char *) malloc(allocSize)) == NULL)
      {
         if (bcmImageGussstimated > 0)
         {
            /* If bcmImageGussstimated is initialized, try that (with allocSize = bcmImageGussstimated)
            * and if still failing,  try reducing 64K from allocSize and try again as long as
            * allocSize > CMS_IMAGE_REQUIRED_LEN.
            */
            while  (((glbCurPtr = (char *) malloc(allocSize)) == NULL) && 
               (allocSize > CMS_IMAGE_REQUIRED_LEN))
            {
               allocSize -= 64 * 1024 ;
               cmsLog_debug("Try allocating %d kb", allocSize/1024);
            }
         }

         /* Failed to allocate memory and will quite tftp operation */
         if (glbCurPtr == NULL)
         {
            bb_error_msg("Not enough memory error.  Could not allocate %u bytes.", allocSize);   
            return -1;
         }
      }         
      bb_error_msg("Memory allocated\n");
      glbImagePtr = glbCurPtr;
   }

   // copy the data from the current packet into our buffer
   if (glbUploadSize + inBufLen <= allocSize)
   {
      memcpy(glbCurPtr, inBuf, inBufLen);
      glbCurPtr += inBufLen;
      glbUploadSize += inBufLen;
   }
   else
   {
       bb_error_msg("Image could not fit into %u byte buffer.\n", allocSize);
       return -1;
   }

   return inBufLen;
}
#endif


//-- from igmp
#include <bits/socket.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#define MAXCTRLSIZE						\
	(sizeof(struct cmsghdr) + sizeof(struct sockaddr_in) +	\
	sizeof(struct cmsghdr) + sizeof(int) + 32)

#ifdef not_used
/*
 * Interesting, tftp uses a different method for getting the 
 * linux interface name. See also cmsImg_saveIfNameFromSocket().
 * Sean says tftp is a client, so it needs to use this method.
 * In all other upload methods, the modem is the server, so the method
 * in cmsImg_saveIfNameFromSocket() is used.
 */
static char *myif_indextoname(int sockfd,unsigned int ifindex,char *ifname)
{
	struct ifreq ifr;
	int status;

	memset(&ifr,0,sizeof(struct ifreq));
 	ifr.ifr_ifindex = ifindex;
	
	status = ioctl(sockfd,SIOCGIFNAME,&ifr);
	
	if (status < 0) {
		//bb_error_msg("ifindex %d has no device \n",ifindex);
		return NULL;
	}
	else
		return strncpy(ifname,ifr.ifr_name,IFNAMSIZ);
}
#endif
// brcm end

#if ENABLE_FEATURE_TFTP_PROGRESS_BAR
/* SIGALRM logic nicked from the wget applet */
static void progress_meter(int flag)
{
	/* We can be called from signal handler */
	int save_errno = errno;

	if (flag == -1) { /* first call to progress_meter */
		bb_progress_init(&G.pmt);
	}

	bb_progress_update(&G.pmt, G.file, 0, G.pos, G.size);

	if (flag == 0) {
		/* last call to progress_meter */
		alarm(0);
		bb_putchar_stderr('\n');
	} else {
		if (flag == -1) { /* first call to progress_meter */
			signal_SA_RESTART_empty_mask(SIGALRM, progress_meter);
		}
		alarm(1);
	}

	errno = save_errno;
}
static void tftp_progress_init(void)
{
	progress_meter(-1);
}
static void tftp_progress_done(void)
{
	if (G.pmt.inited)
		progress_meter(0);
}
#else
# define tftp_progress_init() ((void)0)
# define tftp_progress_done() ((void)0)
#endif

#if ENABLE_FEATURE_TFTP_BLOCKSIZE

static int tftp_blksize_check(const char *blksize_str, int maxsize)
{
	/* Check if the blksize is valid:
	 * RFC2348 says between 8 and 65464,
	 * but our implementation makes it impossible
	 * to use blksizes smaller than 22 octets. */
	unsigned blksize = bb_strtou(blksize_str, NULL, 10);
	if (errno
	 || (blksize < 24) || (blksize > maxsize)
	) {
		bb_error_msg("bad blocksize '%s'", blksize_str);
		return -1;
	}
# if ENABLE_TFTP_DEBUG
	bb_error_msg("using blksize %u", blksize);
# endif
	return blksize;
}

static char *tftp_get_option(const char *option, char *buf, int len)
{
	int opt_val = 0;
	int opt_found = 0;
	int k;

	/* buf points to:
	 * "opt_name<NUL>opt_val<NUL>opt_name2<NUL>opt_val2<NUL>..." */

	while (len > 0) {
		/* Make sure options are terminated correctly */
		for (k = 0; k < len; k++) {
			if (buf[k] == '\0') {
				goto nul_found;
			}
		}
		return NULL;
 nul_found:
		if (opt_val == 0) { /* it's "name" part */
			if (strcasecmp(buf, option) == 0) {
				opt_found = 1;
			}
		} else if (opt_found) {
			return buf;
		}

		k++;
		buf += k;
		len -= k;
		opt_val ^= 1;
	}

	return NULL;
}

#endif

static int tftp_protocol(
		/* NULL if tftp, !NULL if tftpd: */
		len_and_sockaddr *our_lsa,
		len_and_sockaddr *peer_lsa,
		const char *local_file
		IF_TFTP(, const char *remote_file)
#if !ENABLE_TFTP
# define remote_file NULL
#endif
		/* 1 for tftp; 1/0 for tftpd depending whether client asked about it: */
		IF_FEATURE_TFTP_BLOCKSIZE(, int want_transfer_size)
		IF_FEATURE_TFTP_BLOCKSIZE(, int blksize))
{
#if !ENABLE_FEATURE_TFTP_BLOCKSIZE
	enum { blksize = TFTP_BLKSIZE_DEFAULT };
#endif

	struct pollfd pfd[1];
#define socket_fd (pfd[0].fd)
	int len;
	int send_len;
	IF_FEATURE_TFTP_BLOCKSIZE(smallint expect_OACK = 0;)
	smallint finished = 0;
	uint16_t opcode;
	uint16_t block_nr;
	uint16_t recv_blk;
	int open_mode, local_fd = -1; // brcm
	int retries, waittime_ms;
	int io_bufsize = blksize + 4;
	char *cp;
	/* Can't use RESERVE_CONFIG_BUFFER here since the allocation
	 * size varies meaning BUFFERS_GO_ON_STACK would fail.
	 *
	 * We must keep the transmit and receive buffers separate
	 * in case we rcv a garbage pkt - we need to rexmit the last pkt.
	 */
	char *xbuf = xmalloc(io_bufsize);
	char *rbuf = xmalloc(io_bufsize);

	socket_fd = xsocket(peer_lsa->u.sa.sa_family, SOCK_DGRAM, 0);
	setsockopt_reuseaddr(socket_fd);

	if (!ENABLE_TFTP || our_lsa) { /* tftpd */
		/* Create a socket which is:
		 * 1. bound to IP:port peer sent 1st datagram to,
		 * 2. connected to peer's IP:port
		 * This way we will answer from the IP:port peer
		 * expects, will not get any other packets on
		 * the socket, and also plain read/write will work. */
		xbind(socket_fd, &our_lsa->u.sa, our_lsa->len);
		xconnect(socket_fd, &peer_lsa->u.sa, peer_lsa->len);

		/* Is there an error already? Send pkt and bail out */
		if (error_pkt_reason || error_pkt_str[0])
			goto send_err_pkt;

		if (user_opt) {
			struct passwd *pw = xgetpwnam(user_opt);
			change_identity(pw); /* initgroups, setgid, setuid */
		}
	}

	/* Prepare open mode */
	if (CMD_PUT(option_mask32)) {
		open_mode = O_RDONLY;
	} else {
		open_mode = O_WRONLY | O_TRUNC | O_CREAT;
#if !ENABLE_BRCMTFTPD
		if ((option_mask32 & (TFTPD_OPT+TFTPD_OPT_c)) == TFTPD_OPT) {
			/* tftpd without -c */
			open_mode = O_WRONLY | O_TRUNC;
		}
#endif
	}

	/* Examples of network traffic.
	 * Note two cases when ACKs with block# of 0 are sent.
	 *
	 * Download without options:
	 * tftp -> "\0\1FILENAME\0octet\0"
	 *         "\0\3\0\1FILEDATA..." <- tftpd
	 * tftp -> "\0\4\0\1"
	 * ...
	 * Download with option of blksize 16384:
	 * tftp -> "\0\1FILENAME\0octet\0blksize\00016384\0"
	 *         "\0\6blksize\00016384\0" <- tftpd
	 * tftp -> "\0\4\0\0"
	 *         "\0\3\0\1FILEDATA..." <- tftpd
	 * tftp -> "\0\4\0\1"
	 * ...
	 * Upload without options:
	 * tftp -> "\0\2FILENAME\0octet\0"
	 *         "\0\4\0\0" <- tftpd
	 * tftp -> "\0\3\0\1FILEDATA..."
	 *         "\0\4\0\1" <- tftpd
	 * ...
	 * Upload with option of blksize 16384:
	 * tftp -> "\0\2FILENAME\0octet\0blksize\00016384\0"
	 *         "\0\6blksize\00016384\0" <- tftpd
	 * tftp -> "\0\3\0\1FILEDATA..."
	 *         "\0\4\0\1" <- tftpd
	 * ...
	 */
	block_nr = 1;
	cp = xbuf + 2;

	if (!ENABLE_TFTP || our_lsa) { /* tftpd */
		/* Open file (must be after changing user) */
		local_fd = open(local_file, open_mode, 0666);
		if (local_fd < 0) {
			error_pkt_reason = ERR_NOFILE;
			strcpy((char*)error_pkt_str, "can't open file");
			goto send_err_pkt;
		}
/* gcc 4.3.1 would NOT optimize it out as it should! */
#if ENABLE_FEATURE_TFTP_BLOCKSIZE
		if (blksize != TFTP_BLKSIZE_DEFAULT || want_transfer_size) {
			/* Create and send OACK packet. */
			/* For the download case, block_nr is still 1 -
			 * we expect 1st ACK from peer to be for (block_nr-1),
			 * that is, for "block 0" which is our OACK pkt */
			opcode = TFTP_OACK;
			goto add_blksize_opt;
		}
#endif
		if (CMD_GET(option_mask32)) {
			/* It's upload and we don't send OACK.
			 * We must ACK 1st packet (with filename)
			 * as if it is "block 0" */
			block_nr = 0;
		}

	} else { /* tftp */
		/* Open file (must be after changing user) */
		if ( !brcm_tftp ) {
			local_fd = CMD_GET(option_mask32) ? STDOUT_FILENO : STDIN_FILENO;
			if (NOT_LONE_DASH(local_file))
				local_fd = xopen(local_file, open_mode);
		} else {
			/* need to send in the connection interface name to smd */
			if (glbUploadType == 'i')
			{
			   /*
			    * There is a big image coming.  tftp is about to malloc a big buffer
			    * and start filling it.  Notify smd so it can do killAllApps or
			    * something to make memory available on the modem.
			    */
			   cmsImg_sendLoadStartingMsg(msgHandle, connIfName);
			}
		}
/* Removing #if, or using if() statement instead of #if may lead to
 * "warning: null argument where non-null required": */
#if ENABLE_TFTP
		/* tftp */

		/* We can't (and don't really need to) bind the socket:
		 * we don't know from which local IP datagrams will be sent,
		 * but kernel will pick the same IP every time (unless routing
		 * table is changed), thus peer will see dgrams consistently
		 * coming from the same IP.
		 * We would like to connect the socket, but since peer's
		 * UDP code can be less perfect than ours, _peer's_ IP:port
		 * in replies may differ from IP:port we used to send
		 * our first packet. We can connect() only when we get
		 * first reply. */

		/* build opcode */
		opcode = TFTP_WRQ;
		if (CMD_GET(option_mask32)) {
			opcode = TFTP_RRQ;
		}
		/* add filename and mode */
		/* fill in packet if the filename fits into xbuf */
		len = strlen(remote_file) + 1;
		if (2 + len + sizeof("octet") >= io_bufsize) {
			bb_error_msg("remote filename is too long");
			goto ret;
		}
		strcpy(cp, remote_file);
		cp += len;
		/* add "mode" part of the packet */
		strcpy(cp, "octet");
		cp += sizeof("octet");

# if ENABLE_FEATURE_TFTP_BLOCKSIZE
		if (blksize == TFTP_BLKSIZE_DEFAULT && !want_transfer_size)
			goto send_pkt;

		/* Need to add option to pkt */
		if ((&xbuf[io_bufsize - 1] - cp) < sizeof("blksize NNNNN tsize ") + sizeof(off_t)*3) {
			bb_error_msg("remote filename is too long");
			goto ret;
		}
		expect_OACK = 1;
# endif
#endif /* ENABLE_TFTP */

#if ENABLE_FEATURE_TFTP_BLOCKSIZE
 add_blksize_opt:
		if (blksize != TFTP_BLKSIZE_DEFAULT) {
			/* add "blksize", <nul>, blksize, <nul> */
			strcpy(cp, "blksize");
			cp += sizeof("blksize");
			cp += snprintf(cp, 6, "%d", blksize) + 1;
		}
		if (want_transfer_size) {
			/* add "tsize", <nul>, size, <nul> (see RFC2349) */
			/* if tftp and downloading, we send "0" (since we opened local_fd with O_TRUNC)
			 * and this makes server to send "tsize" option with the size */
			/* if tftp and uploading, we send file size (maybe dont, to not confuse old servers???) */
			/* if tftpd and downloading, we are answering to client's request */
			/* if tftpd and uploading: !want_transfer_size, this code is not executed */
			struct stat st;
			strcpy(cp, "tsize");
			cp += sizeof("tsize");
			st.st_size = 0;
			fstat(local_fd, &st);
			cp += sprintf(cp, "%"OFF_FMT"u", (off_t)st.st_size) + 1;
# if ENABLE_FEATURE_TFTP_PROGRESS_BAR
			/* Save for progress bar. If 0 (tftp downloading),
			 * we look at server's reply later */
			G.size = st.st_size;
			if (remote_file && st.st_size)
				tftp_progress_init();
# endif
		}
#endif
		/* First packet is built, so skip packet generation */
		goto send_pkt;
	}

	/* Using mostly goto's - continue/break will be less clear
	 * in where we actually jump to */
	while (1) {
		/* Build ACK or DATA */
		cp = xbuf + 2;
		*((uint16_t*)cp) = htons(block_nr);
		cp += 2;
		block_nr++;
		opcode = TFTP_ACK;
		if (CMD_PUT(option_mask32)) {
			opcode = TFTP_DATA;
			if( brcm_tftp ) {
 				len = myRead(cp, blksize);
			} else {
	  			len = full_read(local_fd, cp, blksize);
			}
			if (len < 0) {
				goto send_read_err_pkt;
			}
			if (len != blksize) {
				finished = 1;
			}
			cp += len;
		}
 send_pkt:
		/* Send packet */
		*((uint16_t*)xbuf) = htons(opcode); /* fill in opcode part */
		send_len = cp - xbuf;
		/* NB: send_len value is preserved in code below
		 * for potential resend */

		retries = TFTP_NUM_RETRIES;	/* re-initialize */
		waittime_ms = TFTP_TIMEOUT_MS;

 send_again:
#if ENABLE_TFTP_DEBUG
		fprintf(stderr, "sending %u bytes\n", send_len);
		for (cp = xbuf; cp < &xbuf[send_len]; cp++)
			fprintf(stderr, "%02x ", (unsigned char) *cp);
		fprintf(stderr, "\n");
#endif
		xsendto(socket_fd, xbuf, send_len, &peer_lsa->u.sa, peer_lsa->len);

#if ENABLE_FEATURE_TFTP_PROGRESS_BAR
		if (ENABLE_TFTP && remote_file) { /* tftp */
			G.pos = (block_nr - 1) * (uoff_t)blksize;
		}
#endif
		/* Was it final ACK? then exit */
		if (finished && (opcode == TFTP_ACK))
			goto ret;

 recv_again:
		/* Receive packet */
		/*pfd[0].fd = socket_fd;*/
		pfd[0].events = POLLIN;
		switch (safe_poll(pfd, 1, waittime_ms)) {
		default:
			/*bb_perror_msg("poll"); - done in safe_poll */
			goto ret;
		case 0:
			retries--;
			if (retries == 0) {
				bb_error_msg("timeout");
				goto ret; /* no err packet sent */
			}

			/* exponential backoff with limit */
			waittime_ms += waittime_ms/2;
			if (waittime_ms > TFTP_MAXTIMEOUT_MS) {
				waittime_ms = TFTP_MAXTIMEOUT_MS;
			}

			goto send_again; /* resend last sent pkt */
		case 1:
			if (!our_lsa) {
				/* tftp (not tftpd!) receiving 1st packet */
				our_lsa = ((void*)(ptrdiff_t)-1); /* not NULL */
				len = recvfrom(socket_fd, rbuf, io_bufsize, 0,
						&peer_lsa->u.sa, &peer_lsa->len);
				/* Our first dgram went to port 69
				 * but reply may come from different one.
				 * Remember and use this new port (and IP) */
				if (len >= 0)
					xconnect(socket_fd, &peer_lsa->u.sa, peer_lsa->len);
			} else {
				/* tftpd, or not the very first packet:
				 * socket is connect()ed, can just read from it. */
				/* Don't full_read()!
				 * This is not TCP, one read == one pkt! */
				len = safe_read(socket_fd, rbuf, io_bufsize);
			}
			if (len < 0) {
				goto send_read_err_pkt;
			}
			if (len < 4) { /* too small? */
				goto recv_again;
			}
		}

		/* Process recv'ed packet */
		opcode = ntohs( ((uint16_t*)rbuf)[0] );
		recv_blk = ntohs( ((uint16_t*)rbuf)[1] );
#if ENABLE_TFTP_DEBUG
		fprintf(stderr, "received %d bytes: %04x %04x\n", len, opcode, recv_blk);
#endif
		if (opcode == TFTP_ERROR) {
			static const char errcode_str[] ALIGN1 =
				"\0"
				"file not found\0"
				"access violation\0"
				"disk full\0"
				"bad operation\0"
				"unknown transfer id\0"
				"file already exists\0"
				"no such user\0"
				"bad option";

			const char *msg = "";

			if (len > 4 && rbuf[4] != '\0') {
				msg = &rbuf[4];
				rbuf[io_bufsize - 1] = '\0'; /* paranoia */
			} else if (recv_blk <= 8) {
				msg = nth_string(errcode_str, recv_blk);
			}
			bb_error_msg("server error: (%u) %s", recv_blk, msg);
			goto ret;
		}

#if ENABLE_FEATURE_TFTP_BLOCKSIZE
		if (expect_OACK) {
			expect_OACK = 0;
			if (opcode == TFTP_OACK) {
				/* server seems to support options */
				char *res;

				res = tftp_get_option("blksize", &rbuf[2], len - 2);
				if (res) {
					blksize = tftp_blksize_check(res, blksize);
					if (blksize < 0) {
						error_pkt_reason = ERR_BAD_OPT;
						goto send_err_pkt;
					}
					io_bufsize = blksize + 4;
				}
# if ENABLE_FEATURE_TFTP_PROGRESS_BAR
				if (remote_file && G.size == 0) { /* if we don't know it yet */
					res = tftp_get_option("tsize", &rbuf[2], len - 2);
					if (res) {
						G.size = bb_strtoull(res, NULL, 10);
						if (G.size)
							tftp_progress_init();
					}
				}
# endif
				if (CMD_GET(option_mask32)) {
					/* We'll send ACK for OACK,
					 * such ACK has "block no" of 0 */
					block_nr = 0;
				}
				continue;
			}
			/* rfc2347:
			 * "An option not acknowledged by the server
			 * must be ignored by the client and server
			 * as if it were never requested." */
			if (blksize != TFTP_BLKSIZE_DEFAULT)
				bb_error_msg("falling back to blocksize "TFTP_BLKSIZE_DEFAULT_STR);
			blksize = TFTP_BLKSIZE_DEFAULT;
			io_bufsize = TFTP_BLKSIZE_DEFAULT + 4;
		}
#endif
		/* block_nr is already advanced to next block# we expect
		 * to get / block# we are about to send next time */

		if (CMD_GET(option_mask32) && (opcode == TFTP_DATA)) {
			if (recv_blk == block_nr) {
// brcm				int sz = full_write(local_fd, &rbuf[4], len - 4);
				int sz; 
				if ( local_fd != -1 ) {
					sz=write(local_fd, &rbuf[4], len - 4);
					glbUploadSize += sz;
				}else{
					sz=myWrite(&rbuf[4], len - 4);
				}
				if (sz != len - 4) {
					strcpy((char*)error_pkt_str, bb_msg_write_error);
					error_pkt_reason = ERR_WRITE;
					goto send_err_pkt;
				}
				if (sz != blksize) {
					finished = 1;
				}
				continue; /* send ACK */
			}
/* Disabled to cope with servers with Sorcerer's Apprentice Syndrome */
#if 0
			if (recv_blk == (block_nr - 1)) {
				/* Server lost our TFTP_ACK.  Resend it */
				block_nr = recv_blk;
				continue;
			}
#endif
		}

		if (CMD_PUT(option_mask32) && (opcode == TFTP_ACK)) {
			/* did peer ACK our last DATA pkt? */
			if (recv_blk == (uint16_t) (block_nr - 1)) {
				if (finished)
					goto ret;
				continue; /* send next block */
			}
		}
		/* Awww... recv'd packet is not recognized! */
		goto recv_again;
		/* why recv_again? - rfc1123 says:
		 * "The sender (i.e., the side originating the DATA packets)
		 *  must never resend the current DATA packet on receipt
		 *  of a duplicate ACK".
		 * DATA pkts are resent ONLY on timeout.
		 * Thus "goto send_again" will ba a bad mistake above.
		 * See:
		 * http://en.wikipedia.org/wiki/Sorcerer's_Apprentice_Syndrome
		 */
	} /* end of "while (1)" */
 ret:
	if (ENABLE_FEATURE_CLEAN_UP) {
		close(local_fd);
		close(socket_fd);
		free(xbuf);
		free(rbuf);
	}
	return finished == 0; /* returns 1 on failure */

 send_read_err_pkt:
	strcpy((char*)error_pkt_str, bb_msg_read_error);
 send_err_pkt:
	if (error_pkt_str[0])
		bb_error_msg("%s", (char*)error_pkt_str);
	error_pkt[1] = TFTP_ERROR;
	xsendto(socket_fd, error_pkt, 4 + 1 + strlen((char*)error_pkt_str),
			&peer_lsa->u.sa, peer_lsa->len);
	return EXIT_FAILURE;
#undef remote_file
}

#if ENABLE_TFTP

int tftp_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tftp_main(int argc UNUSED_PARAM, char **argv)
{
	len_and_sockaddr *peer_lsa;
	const char *local_file = NULL;
	const char *remote_file = NULL;
# if ENABLE_FEATURE_TFTP_BLOCKSIZE
	const char *blksize_str = TFTP_BLKSIZE_DEFAULT_STR;
	int blksize;
# endif
// brcm begin
	const char *brcm_remote_file = NULL;
	const char *upload_type = NULL; 
	const char *brcm_loglevel = NULL;
	CmsLogLevel logLevel=DEFAULT_LOG_LEVEL;
	SINT32 logLevelNum;
	CmsRet ret;
// brcm end
	int result;
	int port;
	IF_GETPUT(int opt;)

	INIT_G();

#if defined(SUPPORT_GPL_1)
#ifdef AEI_DEFAULT_CFG_CUSTOMER
#define CUST_CFG "c"
#else
#define CUST_CFG
#endif

#define CUST_2PARTITION_FALSH   "d:"
	const char *dualPartition = NULL;
#endif

// brcm begin
    cmsLog_init(EID_TFTP);
    cmsLog_setLevel(logLevel);
// brcm end
#if defined AEI_DEFAULT_CFG_CUSTOMER
    glbCustomerSpecifyDefConfigFile = FALSE;
#endif

	/* -p or -g is mandatory, and they are mutually exclusive */
	opt_complementary = "" IF_FEATURE_TFTP_GET("g:") IF_FEATURE_TFTP_PUT("p:")
			IF_GETPUT("g--p:p--g:");

#if defined(AEI_FACTORY_TEST)
    if (system("/bin/mp_is_manu_mode.sh") != 0) {
	    IF_GETPUT(opt =) getopt32(argv,
			IF_FEATURE_TFTP_GET("g") IF_FEATURE_TFTP_PUT("p")
				"l:r:f:t:v" IF_FEATURE_TFTP_BLOCKSIZE("b:"),
			&local_file, &remote_file, &brcm_remote_file, &upload_type, &brcm_loglevel
			IF_FEATURE_TFTP_BLOCKSIZE(, &blksize_str));
    } else {
         IF_GETPUT(opt =) getopt32(argv,
            IF_FEATURE_TFTP_GET("g") IF_FEATURE_TFTP_PUT("p")
#if defined(SUPPORT_GPL_1)
            CUST_CFG CUST_2PARTITION_FALSH "f:t:v:" IF_FEATURE_TFTP_BLOCKSIZE("b:"),
#else
            "l:r:f:t:v" IF_FEATURE_TFTP_BLOCKSIZE("b:"),
#endif
#if defined(SUPPORT_GPL_1)
            &dualPartition,
			&brcm_remote_file, &upload_type, &brcm_loglevel
#else
			&local_file, &remote_file, &brcm_remote_file, &upload_type, &brcm_loglevel
#endif
			IF_FEATURE_TFTP_BLOCKSIZE(, &blksize_str));
    }
#else  /*AEI_FACTORY_TEST*/
	IF_GETPUT(opt =) getopt32(argv,
			IF_FEATURE_TFTP_GET("g") IF_FEATURE_TFTP_PUT("p")
#if defined(SUPPORT_GPL_1)
            CUST_CFG CUST_2PARTITION_FALSH "f:t:v:" IF_FEATURE_TFTP_BLOCKSIZE("b:"),
#else
				"l:r:f:t:v" IF_FEATURE_TFTP_BLOCKSIZE("b:"),
#endif
#if defined(SUPPORT_GPL_1)
            &dualPartition,
			&brcm_remote_file, &upload_type, &brcm_loglevel
#else
			&local_file, &remote_file, &brcm_remote_file, &upload_type, &brcm_loglevel
#endif
			IF_FEATURE_TFTP_BLOCKSIZE(, &blksize_str));
#endif /*AEI_FACTORY_TEST*/
	argv += optind;

# if ENABLE_FEATURE_TFTP_BLOCKSIZE
	/* Check if the blksize is valid:
	 * RFC2348 says between 8 and 65464 */
	blksize = tftp_blksize_check(blksize_str, 65564);
	if (blksize < 0) {
		//bb_error_msg("bad block size");
		return EXIT_FAILURE;
	}
# endif

#if defined AEI_DEFAULT_CFG_CUSTOMER
    if(CMD_CUST_CFG(opt))
    {
        glbCustomerSpecifyDefConfigFile = TRUE;
    }
#endif
// brcm begin
	if ( upload_type && brcm_remote_file ) {
		brcm_tftp = 1;
		glbUploadType = upload_type[0];
		remote_file = brcm_remote_file;
	}
	if (brcm_loglevel) {
		logLevelNum = xatoul_range(brcm_loglevel, 0, 7);
		if (logLevelNum == 0) {
			logLevel = LOG_LEVEL_ERR;
		} else if (logLevelNum == 1) {
			logLevel = LOG_LEVEL_NOTICE;
		} else {
			logLevel = LOG_LEVEL_DEBUG;
		}
		cmsLog_setLevel(logLevel);
	}
// brcm end
 
	if (remote_file) {
		if (!local_file) {
			const char *slash = strrchr(remote_file, '/');
			local_file = slash ? slash + 1 : remote_file;
		}
	} else {
		remote_file = local_file;
	}


	/* Error if filename or host is not known */
	if (!remote_file || !argv[0])
		bb_show_usage();

	port = bb_lookup_port(argv[1], "udp", 69);
	peer_lsa = xhost2sockaddr(argv[0], port);

# if ENABLE_TFTP_DEBUG
	fprintf(stderr, "using server '%s', remote_file '%s', local_file '%s'\n",
			xmalloc_sockaddr2dotted(&peer_lsa->u.sa),
			remote_file, local_file);
# endif

# if ENABLE_FEATURE_TFTP_PROGRESS_BAR
	G.file = remote_file;
# endif
// brcm begin
	/* We need to establish a comm link with smd. */
	if ((ret = cmsMsg_init(EID_TFTP, &msgHandle)) != CMSRET_SUCCESS)
	{
		bb_error_msg("failed to open comm link with smd, tftp failed.");
		return 0;
	}

	if( brcm_tftp ) {
#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
      memset(&imageTransferStats,0,sizeof(CmsImageTransferStats));
      imageTransferStats.startTime = time(NULL);
#endif

      result = tftp_protocol(
                             NULL /*our_lsa*/, peer_lsa,
                             local_file, remote_file
                             IF_FEATURE_TFTP_BLOCKSIZE(, 1 /* want_transfer_size */)
                             IF_FEATURE_TFTP_BLOCKSIZE(, blksize)
                             );

#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
      imageTransferStats.fileSize = glbUploadSize;
      imageTransferStats.completeTime = time(NULL);
      if (result == 0)
      {
         imageTransferStats.faultCode = CMSRET_SUCCESS;
      }
#endif

	   if ( glbUploadType == 'f' ) {
#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
         imageTransferStats.isDownload = FALSE;
         imageTransferStats.fileType = CMS_IMAGE_FORMAT_XML_CFG;
#endif
         cmsLog_debug("sending config to remote file %s", remote_file);
         if (result == 0) {		
            bb_error_msg("backed up config file to %s (%d bytes)\n", remote_file, glbUploadSize);
         } else {
            bb_error_msg("Could not back up config file.\n");
#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
            imageTransferStats.faultCode = CMSRET_UPLOAD_FAILURE;
            strcpy(imageTransferStats.faultStr,"Could not backup config file.");
#endif
         }
#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
         cmsImg_sendAutonomousTransferCompleteMsg(msgHandle,&imageTransferStats);
#endif
	   } else {
#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
         imageTransferStats.isDownload = TRUE;         
         if (glbUploadType == 'i')
         {
            imageTransferStats.fileType = CMS_IMAGE_FORMAT_FLASH;
         }
         else
         {
            imageTransferStats.fileType = CMS_IMAGE_FORMAT_XML_CFG;
         }
#endif

	      if ( result == EXIT_SUCCESS ) {
            bb_error_msg("Got image via tftp, total image size: %d\n", glbUploadSize);

		
            /*
             * cmsImsg_writeImage will determine the image format and write
             * to flash.  If successful, the system will do a sysMipsSoftReset
             * immediately.  So we will not return from this function call.
             * (But on the desktop, this call does return, so we still have to check the
             * return value.)
             */
#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
            cmsImg_storeImageTransferStats(&imageTransferStats);   
#endif
#if defined(AEI_CONFIG_JFFS)
            if(dualPartition &&  glbUploadType == 'i' )
            {
                if ((ret = AEI_writeDualPartition(glbImagePtr, glbUploadSize, msgHandle, atoi(dualPartition))) != CMSRET_SUCCESS)
                {
                    printf("Tftp Image failed: Illegal image.\n");
                }
            }
            else if ((cmsImg_validateImage(glbImagePtr, glbUploadSize, msgHandle)) == CMS_IMAGE_FORMAT_XML_CFG)
            {
                printf("Tftp Image failed: it is a config file.\n");
            }
            else
#endif
            if ((ret = cmsImg_writeImage(glbImagePtr, glbUploadSize, msgHandle)) != CMSRET_SUCCESS)
            {
               bb_error_msg("Tftp Image failed: Illegal image.\n");
#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
               imageTransferStats.faultCode = CMSRET_DOWNLOAD_FAILURE;
               strcpy(imageTransferStats.faultStr,"Illegal image error.");
#endif
            }
            /*
             * If we get here, the image download has failed.  Tell smd to go back
             * to normal operation.
             */
            if (glbImagePtr)
            {
               free(glbImagePtr);
            }

#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
            strcpy(imageTransferStats.faultStr,"Flash image failed.");
            /* send a message to TR69c to do autonmous transfer complete.  If flash were successful, 
             * after reboot the autonomous transfer message would be sent. */
            cmsImg_sendAutonomousTransferCompleteMsg(msgHandle,&imageTransferStats);
#endif

            if (glbUploadType == 'i')
            {
               cmsImg_sendLoadDoneMsg(msgHandle);
            }
	      }
	   }
	} else {
// brcm end
		result = tftp_protocol(
			NULL /*our_lsa*/, peer_lsa,
			local_file, remote_file
			IF_FEATURE_TFTP_BLOCKSIZE(, 1 /* want_transfer_size */)
			IF_FEATURE_TFTP_BLOCKSIZE(, blksize)
		);
		tftp_progress_done();
		if (result != EXIT_SUCCESS && NOT_LONE_DASH(local_file) && CMD_GET(opt)) {
			unlink(local_file);
		}
        }
	return result;
}

#endif /* ENABLE_TFTP */

#if !ENABLE_BRCMTFTPD
int tftpd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tftpd_main(int argc UNUSED_PARAM, char **argv)
{
	len_and_sockaddr *our_lsa;
	len_and_sockaddr *peer_lsa;
	char *local_file, *mode;
	const char *error_msg;
	int opt, result, opcode;
	IF_FEATURE_TFTP_BLOCKSIZE(int blksize = TFTP_BLKSIZE_DEFAULT;)
	IF_FEATURE_TFTP_BLOCKSIZE(int want_transfer_size = 0;)

	INIT_G();

	our_lsa = get_sock_lsa(STDIN_FILENO);
	if (!our_lsa) {
		/* This is confusing:
		 *bb_error_msg_and_die("stdin is not a socket");
		 * Better: */
		bb_show_usage();
		/* Help text says that tftpd must be used as inetd service,
		 * which is by far the most usual cause of get_sock_lsa
		 * failure */
	}
	peer_lsa = xzalloc(LSA_LEN_SIZE + our_lsa->len);
	peer_lsa->len = our_lsa->len;

	/* Shifting to not collide with TFTP_OPTs */
#if defined(SUPPORT_GPL_1)
	opt = option_mask32 = TFTPD_OPT | (getopt32(argv, "rcu:", &user_opt) << (TFTPC_OPT_MAX+1));
#else
	opt = option_mask32 = TFTPD_OPT | (getopt32(argv, "rcu:", &user_opt) << 8);
#endif
	argv += optind;
	if (argv[0])
		xchdir(argv[0]);

	result = recv_from_to(STDIN_FILENO, block_buf, sizeof(block_buf),
			0 /* flags */,
			&peer_lsa->u.sa, &our_lsa->u.sa, our_lsa->len);

	error_msg = "malformed packet";
	opcode = ntohs(*(uint16_t*)block_buf);
	if (result < 4 || result >= sizeof(block_buf)
	 || block_buf[result-1] != '\0'
	 || (IF_FEATURE_TFTP_PUT(opcode != TFTP_RRQ) /* not download */
	     IF_GETPUT(&&)
	     IF_FEATURE_TFTP_GET(opcode != TFTP_WRQ) /* not upload */
	    )
	) {
		goto err;
	}
	local_file = block_buf + 2;
	if (local_file[0] == '.' || strstr(local_file, "/.")) {
		error_msg = "dot in file name";
		goto err;
	}
	mode = local_file + strlen(local_file) + 1;
	if (mode >= block_buf + result || strcmp(mode, "octet") != 0) {
		goto err;
	}
# if ENABLE_FEATURE_TFTP_BLOCKSIZE
	{
		char *res;
		char *opt_str = mode + sizeof("octet");
		int opt_len = block_buf + result - opt_str;
		if (opt_len > 0) {
			res = tftp_get_option("blksize", opt_str, opt_len);
			if (res) {
				blksize = tftp_blksize_check(res, 65564);
				if (blksize < 0) {
					error_pkt_reason = ERR_BAD_OPT;
					/* will just send error pkt */
					goto do_proto;
				}
			}
			if (opcode != TFTP_WRQ /* download? */
			/* did client ask us about file size? */
			 && tftp_get_option("tsize", opt_str, opt_len)
			) {
				want_transfer_size = 1;
			}
		}
	}
# endif

	if (!ENABLE_FEATURE_TFTP_PUT || opcode == TFTP_WRQ) {
		if (opt & TFTPD_OPT_r) {
			/* This would mean "disk full" - not true */
			/*error_pkt_reason = ERR_WRITE;*/
			error_msg = bb_msg_write_error;
			goto err;
		}
		IF_GETPUT(option_mask32 |= TFTP_OPT_GET;) /* will receive file's data */
	} else {
		IF_GETPUT(option_mask32 |= TFTP_OPT_PUT;) /* will send file's data */
	}

	/* NB: if error_pkt_str or error_pkt_reason is set up,
	 * tftp_protocol() just sends one error pkt and returns */

 do_proto:
	close(STDIN_FILENO); /* close old, possibly wildcard socket */
	/* tftp_protocol() will create new one, bound to particular local IP */
	result = tftp_protocol(
		our_lsa, peer_lsa,
		local_file IF_TFTP(, NULL /*remote_file*/)
		IF_FEATURE_TFTP_BLOCKSIZE(, want_transfer_size)
		IF_FEATURE_TFTP_BLOCKSIZE(, blksize)
	);

	return result;
 err:
	strcpy((char*)error_pkt_str, error_msg);
	goto do_proto;
}

#endif /* ENABLE_TFTPD */

#endif /* ENABLE_FEATURE_TFTP_GET || ENABLE_FEATURE_TFTP_PUT */
