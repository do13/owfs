/*
$Id$
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: palfille@earthlink.net
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

#include "owfs_config.h"
#include "ow.h"

static int DS2480_next_both(unsigned char * serialnumber, unsigned char search, const struct parsedname * const pn) ;
static int DS2480_databit(int sendbit, int * getbit) ;
static int DS2480_reset( const struct parsedname * const pn ) ;
static int DS2480_read(unsigned char * const buf, const size_t size ) ;
static int DS2480_write(const unsigned char * const buf, const size_t size ) ;
static int DS2480_sendout_data( const unsigned char * const data , const int len ) ;
static int DS2480_level(int new_level) ;
static int DS2480_PowerByte(const unsigned char byte, const unsigned int delay) ;
static int DS2480_ProgramPulse( void ) ;
static int DS2480_sendout_cmd( const unsigned char * cmd , const int len ) ;
static int DS2480_send_cmd( const unsigned char * const cmd , const int len ) ;
static int DS2480_sendback_cmd( const unsigned char * const cmd , unsigned char * const resp , const int len ) ;
static int DS2480_sendback_data( const unsigned char * const data , unsigned char * const resp , const int len ) ;
static void DS2480_setroutines( struct interface_routines * const f ) ;

static void DS2480_setroutines( struct interface_routines * const f ) {
    f->write = DS2480_write ;
    f->read  = DS2480_read ;
    f->reset = DS2480_reset ;
    f->next_both = DS2480_next_both ;
    f->level = DS2480_level ;
    f->PowerByte = DS2480_PowerByte ;
    f->ProgramPulse = DS2480_ProgramPulse ;
    f->sendback_data = DS2480_sendback_data ;
    f->select        = BUS_select_low ;
}

/* --------------------------- */
/* DS2480 defines from PDkit   */
/* --------------------------- */

// Mode Commands
#define MODE_DATA                      0xE1
#define MODE_COMMAND                   0xE3
#define MODE_STOP_PULSE                0xF1

// Return byte value
#define RB_CHIPID_MASK                 0x1C
#define RB_RESET_MASK                  0x03
#define RB_1WIRESHORT                  0x00
#define RB_PRESENCE                    0x01
#define RB_ALARMPRESENCE               0x02
#define RB_NOPRESENCE                  0x03

#define RB_BIT_MASK                    0x03
#define RB_BIT_ONE                     0x03
#define RB_BIT_ZERO                    0x00

// Masks for all bit ranges
#define CMD_MASK                       0x80
#define FUNCTSEL_MASK                  0x60
#define BITPOL_MASK                    0x10
#define SPEEDSEL_MASK                  0x0C
#define MODSEL_MASK                    0x02
#define PARMSEL_MASK                   0x70
#define PARMSET_MASK                   0x0E

// Command or config bit
#define CMD_COMM                       0x81
#define CMD_CONFIG                     0x01

// Function select bits
#define FUNCTSEL_BIT                   0x00
#define FUNCTSEL_SEARCHON              0x30
#define FUNCTSEL_SEARCHOFF             0x20
#define FUNCTSEL_RESET                 0x40
#define FUNCTSEL_CHMOD                 0x60

// Bit polarity/Pulse voltage bits
#define BITPOL_ONE                     0x10
#define BITPOL_ZERO                    0x00
#define BITPOL_5V                      0x00
#define BITPOL_12V                     0x10

// One Wire speed bits
#define SPEEDSEL_STD                   0x00
#define SPEEDSEL_FLEX                  0x04
#define SPEEDSEL_OD                    0x08
#define SPEEDSEL_PULSE                 0x0C

// Data/Command mode select bits
#define MODSEL_DATA                    0x00
#define MODSEL_COMMAND                 0x02

// 5V Follow Pulse select bits (If 5V pulse
// will be following the next byte or bit.)
#define PRIME5V_TRUE                   0x02
#define PRIME5V_FALSE                  0x00

// Parameter select bits
#define PARMSEL_PARMREAD               0x00
#define PARMSEL_SLEW                   0x10
#define PARMSEL_12VPULSE               0x20
#define PARMSEL_5VPULSE                0x30
#define PARMSEL_WRITE1LOW              0x40
#define PARMSEL_SAMPLEOFFSET           0x50
#define PARMSEL_ACTIVEPULLUPTIME       0x60
#define PARMSEL_BAUDRATE               0x70

// Pull down slew rate.
#define PARMSET_Slew15Vus              0x00
#define PARMSET_Slew2p2Vus             0x02
#define PARMSET_Slew1p65Vus            0x04
#define PARMSET_Slew1p37Vus            0x06
#define PARMSET_Slew1p1Vus             0x08
#define PARMSET_Slew0p83Vus            0x0A
#define PARMSET_Slew0p7Vus             0x0C
#define PARMSET_Slew0p55Vus            0x0E

// 12V programming pulse time table
#define PARMSET_32us                   0x00
#define PARMSET_64us                   0x02
#define PARMSET_128us                  0x04
#define PARMSET_256us                  0x06
#define PARMSET_512us                  0x08
#define PARMSET_1024us                 0x0A
#define PARMSET_2048us                 0x0C
#define PARMSET_infinite               0x0E

// 5V strong pull up pulse time table
#define PARMSET_16p4ms                 0x00
#define PARMSET_65p5ms                 0x02
#define PARMSET_131ms                  0x04
#define PARMSET_262ms                  0x06
#define PARMSET_524ms                  0x08
#define PARMSET_1p05s                  0x0A
#define PARMSET_2p10s                  0x0C
#define PARMSET_infinite               0x0E

// Write 1 low time
#define PARMSET_Write8us               0x00
#define PARMSET_Write9us               0x02
#define PARMSET_Write10us              0x04
#define PARMSET_Write11us              0x06
#define PARMSET_Write12us              0x08
#define PARMSET_Write13us              0x0A
#define PARMSET_Write14us              0x0C
#define PARMSET_Write15us              0x0E

// Data sample offset and Write 0 recovery time
#define PARMSET_SampOff3us             0x00
#define PARMSET_SampOff4us             0x02
#define PARMSET_SampOff5us             0x04
#define PARMSET_SampOff6us             0x06
#define PARMSET_SampOff7us             0x08
#define PARMSET_SampOff8us             0x0A
#define PARMSET_SampOff9us             0x0C
#define PARMSET_SampOff10us            0x0E

// Active pull up on time
#define PARMSET_PullUp0p0us            0x00
#define PARMSET_PullUp0p5us            0x02
#define PARMSET_PullUp1p0us            0x04
#define PARMSET_PullUp1p5us            0x06
#define PARMSET_PullUp2p0us            0x08
#define PARMSET_PullUp2p5us            0x0A
#define PARMSET_PullUp3p0us            0x0C
#define PARMSET_PullUp3p5us            0x0E

// Baud rate bits
#define PARMSET_9600                   0x00
#define PARMSET_19200                  0x02
#define PARMSET_57600                  0x04
#define PARMSET_115200                 0x06

// DS2480B program voltage available
#define DS2480PROG_MASK                0x20

/* Reset and detect a DS2480B */
/* returns 0=good
   DS2480 sendback error
   COM_write error
   -EINVAL baudrate error
   If no detection, try a DS9097 passive port */
int DS2480_detect( void ) {
    unsigned char timing = 0xC1 ;
    int ret ;
    unsigned char setup[] = {
        // set the FLEX configuration parameters
        // default PDSRC = 1.37Vus
        CMD_CONFIG | PARMSEL_SLEW | PARMSET_Slew1p37Vus ,
        // default W1LT = 10us
        CMD_CONFIG | PARMSEL_WRITE1LOW | PARMSET_Write10us ,
        // default DSO/WORT = 8us
        CMD_CONFIG | PARMSEL_SAMPLEOFFSET | PARMSET_SampOff8us ,
        // construct the command to read the baud rate (to test command block)
        CMD_CONFIG | PARMSEL_PARMREAD | (PARMSEL_BAUDRATE >> 3) ,
        // also do 1 bit operation (to test 1-Wire block)
        CMD_COMM | FUNCTSEL_BIT | DS2480_baud(speed) | BITPOL_ONE ,
    } ;

    /* Set up low-level routines */
    DS2480_setroutines( & iroutines ) ;
    /* Reset the bus and adapter */
    DS2480_reset(NULL) ;
//printf("2480Detect reset\n");
    // reset modes
    UMode = MODSEL_COMMAND;
    USpeed = SPEEDSEL_FLEX;

    // set the baud rate to 9600
    COM_speed(B9600);

    // send a break to reset the DS2480
    COM_break() ;

    // delay to let line settle
    UT_delay(2);

    // flush the buffers
    COM_flush();

    // send the timing byte
    if ((ret=DS2480_write(&timing,1))) return ret ;
//printf("2480Detect timing\n");

    // delay to let line settle
    UT_delay(4);

    // flush the buffers
    COM_flush();

    // send the packet
    // read back the response
    if ( (ret=DS2480_sendback_cmd(setup,setup,5)) ) return ret ;
//printf("2480Detect cmd packet\n");

    // look at the baud rate and bit operation
    // to see if the response makes sense
    if (
        ((setup[3] & 0xF1) == 0x00) &&
        ((setup[3] & 0x0E) == DS2480_baud(speed)) &&
        ((setup[4] & 0xF0) == 0x90) &&
        ((setup[4] & 0x0C) == DS2480_baud(speed))
       ) {
//printf("2480Detect response: %2X %2X %2X %2X %2X %2X\n",setup[0],setup[1],setup[2],setup[3],setup[4]);
//printf("2480Detect version=%d\n",Adapter) ;
        /* Apparently need to reset again to get the version number properly */
        DS2480_reset(NULL);
        switch (Adapter) {
        case adapter_DS9097U:
            adapter_name = "DS9097U" ;
            break;
        case adapter_LINK:
            adapter_name = "LINK" ;
            break;
        case adapter_LINK_Multi:
            adapter_name = "MultiLINK" ;
            break;
        }
//printf("2480Detect version=%d\n",Adapter) ;
        return 0 ;
    }
//printf("2480Detect bad echo\n");

    return -EINVAL ;
}

/* returns baud rate variable, no errors */
int DS2480_baud( speed_t baud ) {
    switch ( baud ) {
    case B9600:    return PARMSET_9600    ;
    case B19200:   return PARMSET_19200   ;
    case B57600:   return PARMSET_57600   ;
    case B115200:  return PARMSET_115200  ;
    }
    return PARMSET_9600 ;
}

//--------------------------------------------------------------------------
// Reset all of the devices on the 1-Wire Net and return the result.
//
// WARNING: Without setting the above global (FAMILY_CODE_04_ALARM_TOUCHRESET_COMPLIANCE)
//          to TRUE, this routine will not function correctly on some
//          Alarm reset types of the DS1994/DS1427/DS2404 with
//          Rev 1,2, and 3 of the DS2480/DS2480B.
/* return 0=good
   bad = _level, sendback_cmd
 */
static int DS2480_reset( const struct parsedname * const pn ) {
    int ret ;
    unsigned char buf = (unsigned char)(CMD_COMM | FUNCTSEL_RESET | USpeed) ;

//printf("RESET\n");
    // make sure normal level
    if ( (ret=DS2480_level(MODE_NORMAL)) ) return ret ;

    // flush the buffers
    COM_flush();

    // send the packet
    // read back the 1 byte response
    if ( (ret=DS2480_sendback_cmd(&buf,&buf,1)) ) return ret ;

    /* The adapter type is encode in this response byte */
    /* The known values coorespond to the types in enum adapter_type */
    /* Other values are assigned for adapters that don't have this hardcoded value */
    Adapter = (buf&RB_CHIPID_MASK)>>2 ;

    switch ( buf& RB_RESET_MASK ) {
    case RB_1WIRESHORT:
        syslog(LOG_INFO,"1-wire bus short circuit.\n") ;
        // fall through
    case RB_NOPRESENCE:
        if (pn ) pn->si->AnyDevices = 0 ;
        break ;
    case RB_PRESENCE:
    case RB_ALARMPRESENCE:
        if ( pn ) pn->si->AnyDevices = 1 ;
        // check if programming voltage available
        ProgramAvailable = ((buf & 0x20) == 0x20);
        UT_delay(5); // delay 5 ms to give DS1994 enough time
        COM_flush();
     }
     return 0 ;
}

//--------------------------------------------------------------------------
// Set the 1-Wire Net line level.  The values for new_level are
// as follows:
//
// 'new_level' - new level defined as
//                MODE_NORMAL     0x00
//                MODE_STRONG5    0x02
//                MODE_PROGRAM    0x04
//                MODE_BREAK      0x08 (not supported)
//
//  ULEVEL     - global var - level set

// Returns:    0 GOOD, !0 Error
/* return 0=good
  sendout_cmd,readin
  -EIO response byte doesn't match
 */
static int DS2480_level(int new_level) {
    int ret ;
    if (new_level == ULevel) {     // check if need to change level
        return 0 ;

    } else if (new_level == MODE_NORMAL) {     // check if just putting back to normal
        int docheck=0;
        unsigned char c ;
        // check for disable strong pullup step
        if (ULevel == MODE_STRONG5) docheck = 1 ;

        // check if correct mode
        // stop pulse command
        c = MODE_STOP_PULSE;

        // flush the buffers
        COM_flush();

        // send the packet
        if ( (ret=DS2480_sendout_cmd(&c,1)) ) return ret ;

        UT_delay(4);

        // read back the 1 byte response
        // check response byte
        if ( (ret=DS2480_read(&c,1)) || (ret=((c&0xE0)==0xE0)?0:-EIO) ) return ret ;

        ULevel = MODE_NORMAL;

        // do extra bit for DS2480 disable strong pullup
        if ( !docheck || DS2480_databit(1,&docheck) ) return -EIO ;

    } else if (new_level == MODE_STRONG5) { // strong 5 volts
        unsigned char b[] = {
            // set the SPUD time value
            CMD_CONFIG | PARMSEL_5VPULSE | PARMSET_infinite ,
            // add the command to begin the pulse
            CMD_COMM | FUNCTSEL_CHMOD | SPEEDSEL_PULSE | BITPOL_5V ,
        } ;
        // flush the buffers
        COM_flush();
        // send the packet
        // read back the 1 byte response from setting time limit
        // check response byte
        if ( (ret=DS2480_sendout_cmd(b,2)) || (ret=DS2480_read(b,1)) || (ret=(b[0]&0x81)==0x00?0:-EIO) ) return ret ;

    } else if (new_level == MODE_PROGRAM) { // 12 volts
        unsigned char b[] = {
            // set the PPD time value
            CMD_CONFIG | PARMSEL_12VPULSE | PARMSET_infinite,
            // add the command to begin the pulse
            CMD_COMM | FUNCTSEL_CHMOD | SPEEDSEL_PULSE | BITPOL_12V,
        } ;
        // check if programming voltage available
        if (!ProgramAvailable) return 0 ;
        // flush the buffers
        COM_flush();
        // send the packet
        // read back the 1 byte response from setting time limit
        // check response byte
        if ( (ret=DS2480_sendout_cmd(b,2)) || (ret=DS2480_read(b,1)) || (ret=(b[0]&0x81)==0x00?0:-EIO) ) return ret ;

    }
    ULevel = new_level;
    return 0 ;
}

//--------------------------------------------------------------------------
// Send 1 bit of communication to the 1-Wire Net and get the
// result 1 bit read from the 1-Wire Net.  The parameter 'sendbit'
// least significant bit is used and the least significant bit
// of the response is the return bit.
//
// 'sendbit' - the least significant bit is the bit to send
//
// 'getbit' - the least significant bit is the bit received
/* return 0=good
   -EIO bad
 */
static int DS2480_databit(int sendbit, int * getbit) {
    unsigned char readbuffer[10],sendpacket[10];
    int ret ;
    unsigned int sendlen=0;

    // make sure normal level
    if ( (ret=DS2480_level(MODE_NORMAL)) ) return ret ;

    // check if correct mode
    if (UMode != MODSEL_COMMAND)
    {
       UMode = MODSEL_COMMAND;
       sendpacket[sendlen++] = MODE_COMMAND;
    }

    // construct the command
    sendpacket[sendlen] = (sendbit != 0) ? BITPOL_ONE : BITPOL_ZERO;
    sendpacket[sendlen++] |= CMD_COMM | FUNCTSEL_BIT | USpeed;

    // flush the buffers
    COM_flush();

    // send the packet
    if ( (ret=DS2480_write(sendpacket,sendlen)) || (ret=DS2480_read(readbuffer,1)) ) return ret ;

    // interpret the response
    *getbit = ((readbuffer[0] & 0xE0) == 0x80) && ((readbuffer[0] & RB_BIT_MASK) == RB_BIT_ONE) ;

   return 0;
}

/* search = 0xF0 normal 0xEC alarm */
static int DS2480_next_both(unsigned char * serialnumber, unsigned char search, const struct parsedname * const pn) {
    int ret ;
    int mismatched;
    struct stateinfo * si = pn->si ;
    unsigned char sn[8];
    unsigned char bitpairs[16];
    unsigned char searchon  = (unsigned char)(CMD_COMM | FUNCTSEL_SEARCHON  | USpeed);
    unsigned char searchoff = (unsigned char)(CMD_COMM | FUNCTSEL_SEARCHOFF | USpeed);
    int i ;

//printf("NEXT\n");
    if ( !si->AnyDevices ) si->LastDevice = 1 ;
    if ( si->LastDevice ) return -ENODEV ;

    // build the command stream
    // call a function that may add the change mode command to the buff
    // check if correct mode
    // issue the search command
    // change back to command mode
    // search mode on
    // change back to data mode

    // set the temp Last Descrep to none
    mismatched = -1;

    // add the 16 bytes of the search
    memset( bitpairs,0,16) ;

    // set the bits in the added buffer
    for (i = 0; i < si->LastDiscrepancy; i++) {
        // before last discrepancy
        UT_set2bit( bitpairs,i,UT_getbit(serialnumber,i)<<1 ) ;
    }
    // at last discrepancy
    if (si->LastDiscrepancy > -1 ) UT_set2bit( bitpairs,si->LastDiscrepancy,1<<1 ) ;
    // after last discrepancy so leave zeros

    // flush the buffers
    COM_flush();

    // search ON
    // change back to command mode
    // send the packet
    // search OFF
    if ( (ret=BUS_send_data( &search,1 )) || (ret=DS2480_sendout_cmd( &searchon,1 )) || (ret=BUS_sendback_data( bitpairs,bitpairs,16 )) || (ret=DS2480_sendout_cmd( &searchoff,1 )) ) return ret ;

    // interpret the bit stream
    for (i = 0; i < 64; i++) {
        // get the SerialNum bit
        UT_setbit(sn,i,UT_get2bit(bitpairs,i)>>1) ;
        // check LastDiscrepancy
        if ( UT_get2bit(bitpairs,i)==0x1 ) {
            mismatched = i ;
            // check LastFamilyDiscrepancy
            if (i < 8) si->LastFamilyDiscrepancy = i ;
        }
    }

    // CRC check
    if ( CRC8(sn,8) || (si->LastDiscrepancy == 63) || (sn[0] == 0)) return -EIO ;

    // successful search
    // check for last one
    if ((mismatched == si->LastDiscrepancy) || (mismatched == -1)) si->LastDevice = 1 ;

    // copy the SerialNum to the buffer
    memcpy(serialnumber,sn,8) ;

    // set the count
    si->LastDiscrepancy = mismatched;

   return 0 ;
}
//--------------------------------------------------------------------------
// Send 8 bits of communication to the 1-Wire Net and verify that the
// 8 bits read from the 1-Wire Net is the same (write operation).
// The parameter 'byte' least significant 8 bits are used.  After the
// 8 bits are sent change the level of the 1-Wire net.
// Delay delay msec and return to normal
//
/* Returns 0=good
   bad = -EIO
 */
static int DS2480_PowerByte(const unsigned char byte, const unsigned int delay) {
    int ret ;
    unsigned char cmd[] = {
        // set the SPUD time value
        CMD_CONFIG | PARMSEL_5VPULSE | PARMSET_infinite ,
        // bit 1
        ((byte & 0x01) ? BITPOL_ONE : BITPOL_ZERO) | CMD_COMM | FUNCTSEL_BIT | USpeed | PRIME5V_FALSE ,
        // bit 2
        ((byte & 0x02) ? BITPOL_ONE : BITPOL_ZERO) | CMD_COMM | FUNCTSEL_BIT | USpeed | PRIME5V_FALSE ,
        // bit 3
        ((byte & 0x04) ? BITPOL_ONE : BITPOL_ZERO) | CMD_COMM | FUNCTSEL_BIT | USpeed | PRIME5V_FALSE ,
        // bit 4
        ((byte & 0x08) ? BITPOL_ONE : BITPOL_ZERO) | CMD_COMM | FUNCTSEL_BIT | USpeed | PRIME5V_FALSE ,
        // bit 5
        ((byte & 0x10) ? BITPOL_ONE : BITPOL_ZERO) | CMD_COMM | FUNCTSEL_BIT | USpeed | PRIME5V_FALSE ,
        // bit 6
        ((byte & 0x20) ? BITPOL_ONE : BITPOL_ZERO) | CMD_COMM | FUNCTSEL_BIT | USpeed | PRIME5V_FALSE ,
        // bit 7
        ((byte & 0x40) ? BITPOL_ONE : BITPOL_ZERO) | CMD_COMM | FUNCTSEL_BIT | USpeed | PRIME5V_FALSE ,
        // bit 8
        ((byte & 0x80) ? BITPOL_ONE : BITPOL_ZERO) | CMD_COMM | FUNCTSEL_BIT | USpeed | PRIME5V_TRUE ,
    } ;
    unsigned char resp[9] ;

    // flush the buffers
    COM_flush();

    // send the packet
    // read back the 9 byte response from setting time limit
    if ( (ret=DS2480_sendback_cmd(cmd,resp,9)) || (ret=(resp[0]&0x81)?-EIO:0) ) return ret ;
//printf("Sendback byte=%.2X Resp=%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X Cmd=%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X \n",byte,resp[1],resp[2],resp[3],resp[4],resp[5],resp[6],resp[7],resp[8],cmd[1],cmd[2],cmd[3],cmd[4],cmd[5],cmd[6],cmd[7],cmd[8]) ;
//printf("All=%.2X\n",((resp[8]&1)<<7) | ((resp[7]&1)<<6) | ((resp[6]&1)<<5) | ((resp[5]&1)<<4) | ((resp[4]&1)<<3) | ((resp[3]&1)<<2) | ((resp[2]&1)<<1) | (resp[1]&1) );

// indicate the port is now at power delivery
    ULevel = MODE_STRONG5;

    // check the response bit
    ret = byte ^ ( ((resp[8]&1)<<7) | ((resp[7]&1)<<6) | ((resp[6]&1)<<5) | ((resp[5]&1)<<4) | ((resp[4]&1)<<3) | ((resp[3]&1)<<2) | ((resp[2]&1)<<1) | (resp[1]&1) ) ;
    if ( ret ) return ret ;

    // delay
    UT_delay( delay ) ;

    // return to normal level
   return DS2480_level(MODE_NORMAL) ;
}

/* Send a 12v 480usec pulse on the 1wire bus to program the EPROM */
/* Note, DS2480_reset must have been called at once in the past for ProgramAvailable setting */
/* returns 0 if good
   -EINVAL if not program pulse available
   -EIO on error
 */
static int DS2480_ProgramPulse( void ) {
    int ret ;
    unsigned char cmd[] = { CMD_CONFIG|PARMSEL_12VPULSE|PARMSET_512us, CMD_COMM|FUNCTSEL_CHMOD|BITPOL_12V|SPEEDSEL_PULSE, } ;
    unsigned char resp[2] ;
    COM_flush() ;
    (ret=ProgramAvailable?0:-EINVAL) || (ret=DS2480_level(MODE_NORMAL)) || (ret=DS2480_sendback_cmd(cmd,resp,2)) || (ret=DS2480_read(resp,2)) || ((cmd[0]==resp[0])?0:-EIO) || (ret=((cmd[1]&0xFC)==(resp[1]&0xFC))?0:-EIO ) ;
    return ret ;
}

//
// Write a string to the serial port
/* return 0=good,
          -EIO = error
 */
static int DS2480_write(const unsigned char *const buf, const size_t size ) {
    ssize_t r = write(devfd,buf,size) ;
    tcdrain(devfd) ;
    return (r!=(ssize_t)size)?-EIO:0 ;
}

/* Assymetric */
/* Read from DS2480 with timeout on each character */
// NOTE: from PDkit, reead 1-byte at a time
// NOTE: change timeout to 40msec from 10msec for LINK
// returns 0=good 1=bad
/* return 0=good,
          -errno = read error
          -EINTR = timeout
 */
static int DS2480_read(unsigned char * const buf, const size_t size ) {
    fd_set fd;
    struct timeval tval;
    int cnt;

    // loop to wait until each byte is available and read it
    for (cnt = 0; cnt < size; cnt++)
    {
        // set a descriptor to wait for a character available
        FD_ZERO(&fd);
        FD_SET(devfd,&fd);
        // set timeout to 40ms
        // NOTE: LINK needs 40msec, not 10 msec!
        tval.tv_sec = 0;
        tval.tv_usec = 40000;

        // if byte available read or return bytes read
        if (select(devfd+1,&fd,NULL,NULL,&tval) != 0) {
            if ( read(devfd,&buf[cnt],1)!= 1 ) return -errno ;
        } else {
            STATLOCK
                ++read_timeout ; /* statistics */
            STATUNLOCK
            buf[cnt] = '\0' ;
            return -EINTR;
        }
   }
   return 0;
}
//
// DS2480_sendout_cmd
//  Send a command but expect no response
//  puts into command mode if needed.
/* return 0=good
   COM_write
 */
static int DS2480_sendout_cmd( const unsigned char * cmd , const int len ) {
/*
    if ( len > UART_FIFO_SIZE-1 ) return DS2480_sendout_cmd(cmd,UART_FIFO_SIZE-1) || DS2480_sendout_cmd(&cmd[UART_FIFO_SIZE-1],len-(UART_FIFO_SIZE-1)) ;
    if ( UMode != MODSEL_COMMAND ) {
        combuffer[0] = MODE_COMMAND ;
    memcpy( &combuffer[1], cmd , len ) ;
        UMode = MODSEL_COMMAND;
    return BUS_send_and_get( combuffer,len+1,NULL,0) ;
    }
    return BUS_send_and_get( cmd , len , NULL , 0 ) ;
}
*/
    int ret ;
    unsigned char mc = MODE_COMMAND ;
    if ( UMode != MODSEL_COMMAND ) {
        // change back to command mode
        UMode = MODSEL_COMMAND;
        (ret=DS2480_write( &mc , 1 )) || (ret= DS2480_write( cmd , (unsigned)len )) ;
    } else {
        ret=DS2480_write( cmd , (unsigned)len ) ;
    }
    return ret ;
}


//
// DS2480_send_cmd
//  Send a command and expect response match
//  puts into command mode if needed.
/* return 0=good
   send_cmd,sendout_cmd,readin
   -EIO if no match
 */
static int DS2480_send_cmd( const unsigned char * const cmd , const int len ) {
    int ret ;
    if ( len>16 ) {
        int clen = len-(len>>1) ;
        (ret=DS2480_send_cmd(cmd,clen)) || (ret=DS2480_send_cmd(&cmd[clen],len>>1)) ;
    } else {
        unsigned char resp[16] ;
        (ret=DS2480_sendback_cmd( cmd , resp , len )) ||  (ret=memcmp(cmd,resp,(size_t)len)?-EIO:0) ;
    }
    return ret ;
}

//
// DS2480_sendback_cmd
//  Send a command and return response block
//  puts into command mode if needed.
/* return 0=good
   sendback_cmd,sendout_cmd,readin
 */
static int DS2480_sendback_cmd( const unsigned char * const cmd , unsigned char * const resp , const int len ) {
    int ret ;
    if ( len>16 ) {
        int clen = len-(len>>1) ;
        (ret=DS2480_sendback_cmd(cmd,resp,clen)) || (ret=DS2480_sendback_cmd(&cmd[clen],&resp[clen],len>>1)) ;
    } else {
        (ret=DS2480_sendout_cmd( cmd , len )) || (ret=DS2480_read( resp , len )) ;
    }
    return ret ;
}


// DS2480_sendout_data
//  Send data but expect no response
//  puts into data mode if needed.
// Repeat magic MODE_COMMAND byte to show true data
/* return 0=good
   COM_write, sendout_data
 */
static int DS2480_sendout_data( const unsigned char * const data , const int len ) {
    int ret ;
    if ( UMode != MODSEL_DATA ) {
        unsigned char md = MODE_DATA ;
        // change back to command mode
        UMode = MODSEL_DATA;
        if ( (ret=DS2480_write( &md , 1 )) )  return ret ;
    }
    if ( len>16 ) {
        int dlen = len-(len>>1) ;
        (ret=DS2480_sendout_data(data,dlen)) || (ret=DS2480_sendout_data(&data[dlen],len>>1)) ;
    } else {
        unsigned char data2[32] ;
        int i ;
        unsigned int j=0 ;
        for ( i=0 ; i<len ; ++i ) {
            data2[j++]=data[i] ;
            if ( data[i] == MODE_COMMAND ) data2[j++] = MODE_COMMAND ;
        }
        ret = DS2480_write( data2 , j ) ;
    }
    return ret ;
}

//
// DS2480_sendback_data
//  Send data and return response block
//  puts into data mode if needed.
/* return 0=good
   sendout_data, readin
 */
static int DS2480_sendback_data( const unsigned char * const data , unsigned char * const resp , const int len ) {
    int ret ;
    (ret=DS2480_sendout_data( data , len )) || (ret=DS2480_read( resp , len )) ;
    return ret ;
}

