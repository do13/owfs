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

/*
 * HEXVALUE() is defined in <readline/chardefs.h>, but since it's more
 * common with 0-9 I changed compare-order in the define
 */
#define _HEXVALUE(c) \
  (((c) >= '0' && (c) <= '9') \
        ? (c)-'0' \
        : (c) >= 'A' && (c) <= 'F' ? (c)-'A'+10 : (c)-'a'+10 )

/* 2 hex digits to number. This is the most used function in owfs for the LINK */
unsigned char string2num( const char * s ) {
    if ( s == NULL ) return 0 ;
    return (_HEXVALUE(s[0]) << 4) + _HEXVALUE(s[1]);
}


#define _TOHEXCHAR(c) (((c) >= 0 && (c) <= 9) ? '0'+(c) : 'A'+(c)-10 )

#if 0
/* number to a hex digit */
char num2char( const unsigned char n ) {
    return _TOHEXCHAR(n);
}
#endif

/* number to 2 hex digits */
void num2string( char * s , const unsigned char n ) {
    s[0] = _TOHEXCHAR(n >> 4) ;
    s[1] = _TOHEXCHAR(n & 0x0F) ;
}

/* 2x hex digits to x number bytes */
void string2bytes( const char * str , unsigned char * b , const int bytes ) {
    int i ;
    for ( i=0 ; i<bytes ; ++i ) b[i]=string2num(&str[i<<1]) ;
}
/* number(x bytes) to 2x hex digits */
void bytes2string( char * str , const unsigned char * b , const int bytes ) {
    int i ;
    for ( i=0 ; i<bytes ; ++i ) num2string(&str[i<<1],b[i]) ;
}

// #define UT_getbit(buf, loc)	( ( (buf)[(loc)>>3]>>((loc)&0x7) ) &0x01 )
int UT_getbit(const unsigned char * buf, const int loc) {
    return ( ( (buf[loc>>3]) >> (loc&0x7) ) & 0x01 ) ;
}
int UT_get2bit(const unsigned char * buf, const int loc) {
    return ( ( (buf[loc>>2]) >> ((loc&0x3)<<1) ) & 0x03 ) ;
}
void UT_setbit( unsigned char * buf, const int loc , const int bit ) {
    if (bit) {
        buf[loc>>3] |= 0x01 << (loc&0x7) ;
    } else {
        buf[loc>>3] &= ~( 0x01 << (loc&0x7) ) ;
    }
}
void UT_set2bit( unsigned char * buf, const int loc , const int bits ) {
    unsigned char * p = &buf[loc>>2] ;
    switch (loc&3) {
    case 0 :
        *p = (*p & 0xFC) | bits ;
        return ;
    case 1 :
        *p = (*p & 0xF3) | (bits<<2) ;
        return ;
    case 2 :
        *p = (*p & 0xCF) | (bits<<4) ;
        return ;
    case 3 :
        *p = (*p & 0x3F) | (bits<<6) ;
        return ;
    }
}

void UT_fromDate( const DATE d, unsigned char * data) {
    data[0] = d & 0xFF ;
    data[1] = (d>>8) & 0xFF ;
    data[2] = (d>>16) & 0xFF ;
    data[3] = (d>>24) & 0xFF ;
}

DATE UT_toDate( const unsigned char * data ) {
    return (((((((unsigned int) data[3])<<8)|data[2])<<8)|data[1])<<8)|data[0] ;
}

