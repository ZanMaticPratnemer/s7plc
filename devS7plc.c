/* $Author: zimoch $ */
/* $Date: 2005/02/07 16:06:41 $ */
/* $Id: devS7plc.c,v 1.1 2005/02/07 16:06:41 zimoch Exp $ */
/* $Name:  $ */
/* $Revision: 1.1 $ */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include <alarm.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <devSup.h>
#include <devLib.h>

#include <biRecord.h>
#include <boRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <mbbiDirectRecord.h>
#include <mbboDirectRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <waveformRecord.h>

#include <drvS7plc.h>
#include <ticp.h>

#if (EPICS_REVISION<14)
/* R3.13 */
#include "compat3_13.h"
#else
/* R3.14 */
#include <cantProceed.h>
#include <epicsExport.h>
#endif

#define S7MEM_TIME 100

typedef struct {              /* Private structure to save IO arguments */
    s7plcStation *station;    /* Card id */
    unsigned short st_offs;   /* Offset (in bytes) within station memory */
    unsigned short bit;       /* Bit number (0-15) for bi/bo */
    unsigned short dtype;     /* Data type */
    unsigned short dlen;      /* Data length (in bytes) */
    epicsInt32 hwLow;         /* Hardware Low limit */
    epicsInt32 hwHigh;        /* Hardware High limit */
} S7memPrivate_t;

static char cvsid_devS7plc[] =
    "$Id: devS7plc.c,v 1.1 2005/02/07 16:06:41 zimoch Exp $";

static long s7plcReport();

static int ioParse(char* recordName, char *parameters, S7memPrivate_t *);

struct devsup {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN io;
};

/* bi for status bit ************************************************/

static long s7plcInitRecordStat(biRecord *);
static long s7plcReadStat(biRecord *);

struct devsup s7plcStat =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordStat,
    NULL,
    s7plcReadStat
};

epicsExportAddress(dset, s7plcStat);

/* bi ***************************************************************/

static long s7plcInitRecordBi(biRecord *);
static long s7plcReadBi(biRecord *);

struct devsup s7plcBi =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordBi,
    NULL,
    s7plcReadBi
};

epicsExportAddress(dset, s7plcBi);

/* bo ***************************************************************/

static long s7plcInitRecordBo(boRecord *);
static long s7plcWriteBo(boRecord *);

struct devsup s7plcBo =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordBo,
    NULL,
    s7plcWriteBo
};

epicsExportAddress(dset, s7plcBo);

/* mbbi *************************************************************/

static long s7plcInitRecordMbbi(mbbiRecord *);
static long s7plcReadMbbi(mbbiRecord *);

struct devsup s7plcMbbi =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordMbbi,
    NULL,
    s7plcReadMbbi
};

epicsExportAddress(dset, s7plcMbbi);

/* mbbo *************************************************************/

static long s7plcInitRecordMbbo(mbboRecord *);
static long s7plcWriteMbbo(mbboRecord *);

struct devsup s7plcMbbo =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordMbbo,
    NULL,
    s7plcWriteMbbo
};

epicsExportAddress(dset, s7plcMbbo);

/* mbbiDirect *******************************************************/

static long s7plcInitRecordMbbiDirect(mbbiDirectRecord *);
static long s7plcReadMbbiDirect(mbbiDirectRecord *);

struct devsup s7plcMbbiDirect =
{
    5,
    s7plcReport,
    NULL,
    s7plcInitRecordMbbiDirect,
    NULL,
    s7plcReadMbbiDirect
};

epicsExportAddress(dset, s7plcMbbiDirect);

/* mbboDirect *******************************************************/

static long s7plcInitRecordMbboDirect(mbboDirectRecord *);
static long s7plcWriteMbboDirect(mbboDirectRecord *);

struct devsup s7plcMbboDirect =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordMbboDirect,
    NULL,
    s7plcWriteMbboDirect
};

epicsExportAddress(dset, s7plcMbboDirect);

/* longin ***********************************************************/

static long s7plcInitRecordLongin(longinRecord *);
static long s7plcReadLongin(longinRecord *);

struct devsup s7plcLongin =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordLongin,
    NULL,
    s7plcReadLongin
};

epicsExportAddress(dset, s7plcLongin);

/* longout **********************************************************/

static long s7plcInitRecordLongout(longoutRecord *);
static long s7plcWriteLongout(longoutRecord *);

struct devsup s7plcLongout =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordLongout,
    NULL,
    s7plcWriteLongout
};

epicsExportAddress(dset, s7plcLongout);

/* ai ***************************************************************/

static long s7plcInitRecordAi(aiRecord *);
static long s7plcReadAi(aiRecord *);
static long s7plcSpecialLinconvAi(aiRecord *, int after);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
    DEVSUPFUN special_linconv;
} s7plcAi =
{
    6,
    NULL,
    NULL,
    s7plcInitRecordAi,
    NULL,
    s7plcReadAi,
    s7plcSpecialLinconvAi
};

epicsExportAddress(dset, s7plcAi);

/* ao ***************************************************************/

static long s7plcInitRecordAo(aoRecord *);
static long s7plcWriteAo(aoRecord *);
static long s7plcSpecialLinconvAo(aoRecord *, int after);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
    DEVSUPFUN special_linconv;
} s7plcAo =
{
    6,
    NULL,
    NULL,
    s7plcInitRecordAo,
    NULL,
    s7plcWriteAo,
    s7plcSpecialLinconvAo
};

epicsExportAddress(dset, s7plcAo);

/* stringin *********************************************************/

static long s7plcInitRecordStringin(stringinRecord *);
static long s7plcReadStringin(stringinRecord *);

struct devsup s7plcStringin =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordStringin,
    NULL,
    s7plcReadStringin
};

epicsExportAddress(dset, s7plcStringin);

/* stringout ********************************************************/

static long s7plcInitRecordStringout(stringoutRecord *);
static long s7plcWriteStringout(stringoutRecord *);

struct devsup s7plcStringout =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordStringout,
    NULL,
    s7plcWriteStringout
};

epicsExportAddress(dset, s7plcStringout);

/* waveform *********************************************************/

static long s7plcInitRecordWaveform(waveformRecord *);
static long s7plcReadWaveform(waveformRecord *);

struct devsup s7plcWaveform =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordWaveform,
    NULL,
    s7plcReadWaveform
};

epicsExportAddress(dset, s7plcWaveform);

/*********  Report routine ********************************************/

static long s7plcReport()
{
   printf("devS7mem version: %s\n", cvsid_devS7plc);
   return 0;
}

/***********************************************************************
 *   Routine to parse IO arguments
 *   IO address line format:
 *
 *        devName:N/I[+O] [T=<datatype>] [B=<bitnumber>] [L=<hwLow|strLen>] [H=<hwHigh>]
 *
 *   where: devName - symbolic device name
 *            N - station number (0-15)
 *            I - offset within station memory (in bytes)
 *            O - added to get final offset (I+O < 1024)
 *            params    - parameters to be passed to a particular
 *                        devSup parsering routine
 *            datatype  - INT8,INT16,INT32,UNSIGN16,UNSIGN32,
 *                        FLOAT,DOUBLE,STRING,TIME
 *            bitnumber - least significant bit is 0
 *            hwLow     - raw value that mapps to EGUL
 *            hwHigh    - raw value that mapps to EGUF
 **********************************************************************/

static int ioParse(char* recordName, char *par, S7memPrivate_t *priv)
{
    char devName[MAX_DEVNAME_SIZE + 1];
    char *p = par, separator;
    int nchar;
    unsigned int stationNumber;
    int status = 0;

    /* Get rid of leading whitespace and non-alphanumeric chars */
    while (!isalnum((unsigned char)*p)) if (*p++ == '\0') return S_drv_badParam;

    /* Get device name */
    nchar = strcspn(p, ":");
    if (nchar > MAX_DEVNAME_SIZE)
    {
        errlogSevPrintf(errlogFatal, "ioParse %s: device name too long\n",
            recordName);
        return S_drv_badParam;
    }
    strncpy(devName, p, nchar);
    devName[nchar] = '\0';
    p += nchar + 1;

    /* Get station number */
    stationNumber = strtol(p, &p, 0);
    separator = *p++;
    s7plcDebugLog(1, "ioParse %s: station=%d\n", recordName, stationNumber);

    priv->station = s7plcOpen(devName, stationNumber);
    if (!priv->station)
    {
        errlogSevPrintf(errlogFatal, "ioParse %s: device not found\n",
            recordName);
        return S_drv_noDevice;
    }

    /* Check station offset */
    if (separator == '/')
    {
        priv->st_offs = strtol(p, &p, 0);
        separator = *p++;
        /* Handle any number of optional +<n> additions to the st_offs */
        while (separator == '+')
        {
            priv->st_offs += strtol(p, &p, 0);
            separator = *p++;
        }
    }
    else
    {
        priv->st_offs = 0;
    }

    s7plcDebugLog(1, "ioParse %s: st_offs=%d\n", recordName, priv->st_offs);

    /* allow whitespaces before parameter for device support */
    while ((separator == '\t') || (separator == ' '))
        separator = *p++;

    /* handle parameter for device support if present */
    nchar = 0;
    if (separator != '\'') p--; /* quote is optional*/
    
    if (!priv->dtype && !priv->dlen)
    {
        /* set default data type */
        priv->dtype = epicsInt16T;
        priv->dlen = 2;
    }
    priv->bit = 0;
    priv->hwLow = 0;
    priv->hwHigh = 0;
    
    while (p && *p)
    {
        switch (*p)
        {
            case ' ':
            case '\t':
                p++;
                break;
            case 'T': /* T=<datatype> */
                p+=2; 
                if (strncmp(p,"INT8",4) == 0)
                { 
                    priv->dtype = epicsInt8T;
                    priv->dlen = 1;
                    p += 4; 
                }
                else
                if (strncmp(p,"UINT8",5) == 0)
                { 
                    priv->dtype = epicsUInt8T;
                    priv->dlen = 1;
                    p += 5; 
                }
                else
                if (strncmp(p,"UNSIGN8",7) == 0)
                { 
                    priv->dtype = epicsUInt8T;
                    priv->dlen = 1;
                    p += 7; 
                }
                else
                if (strncmp(p,"INT16",5) == 0)
                {
                    priv->dtype = epicsInt16T;
                    priv->dlen = 2; 
                    p += 5;
                }
                else
                if (strncmp(p,"UINT16",6) == 0)
                {
                    priv->dtype = epicsUInt16T;
                    priv->dlen = 2;
                    p += 6;
                }
                else
                if (strncmp(p,"UNSIGN16",8) == 0)
                {
                    priv->dtype = epicsUInt16T;
                    priv->dlen = 2;
                    p += 8;
                }
                else
                if (strncmp(p,"INT32",5) == 0)
                {
                    priv->dtype = epicsInt32T;
                    priv->dlen = 4;
                    p += 5;
                }
                else
                if (strncmp(p,"UINT32",6) == 0)
                {
                    priv->dtype = epicsUInt32T;
                    priv->dlen = 4;
                    p += 6;
                }
                else
                if (strncmp(p,"UNSIGN32",8) == 0)
                {
                    priv->dtype = epicsUInt32T;
                    priv->dlen = 4;
                    p += 8;
                }
                else
                if (strncmp(p,"FLOAT",5) == 0)
                {
                    priv->dtype = epicsFloat32T;
                    priv->dlen = 4;
                    p += 5;
                }
                else
                if (strncmp(p,"DOUBLE",6) == 0)
                {
                    priv->dtype = epicsFloat64T;
                    priv->dlen = 8;
                    p += 6;
                }
                else
                if (strncmp(p,"STRING",6) == 0)
                {
                    priv->dtype = epicsStringT;
                    p += 6;
                }
                else
                if (strncmp(p,"TIME",4) == 0)
                {
                    priv->dtype = S7MEM_TIME;
                    priv->dlen = 1;
                    p += 4;
                }
                else
                {
                    errlogSevPrintf(errlogFatal,
                        "ioParse %s: invalid datatype %s\n",
                        recordName, p);
                    return S_drv_badParam;
                }
                break;
            case 'B': /* B=<bitnumber> */
                p += 2;
                priv->bit = strtol(p,&p,0);
                break;
            case 'L': /* L=<raw low value> (converts to EGUL)*/
                p += 2;
                priv->hwLow = strtol(p,&p,0);
                break;
            case 'H': /* L=<raw high value> (converts to EGUF)*/
                p += 2;
                priv->hwHigh = strtol(p,&p,0);
                break;
            case '\'':
                if (separator == '\'')
                {
                    p = 0;
                    break;
                }
            default:
                errlogSevPrintf(errlogFatal, "ioParse %s: unknown parameter '%c'\n",
                    recordName, *p);
                return S_drv_badParam;
        }
    }
    
    if (priv->dtype == epicsStringT && priv->hwLow)
    {
        /* for STRING L=... means length, not low */
        priv->dlen = priv->hwLow;
        priv->hwLow = 0;
    }
    
    if (priv->bit && priv->bit >= priv->dlen*8)
    {
        errlogSevPrintf(errlogFatal, "ioParse %s: invalid bit number %d (>%d)\n",
            recordName, priv->bit, priv->dlen*8-1);
        return S_drv_badParam;
    }
    
    switch (priv->dtype)
    {
        case epicsUInt8T:
            if (priv->hwHigh > 0xFF) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0x00;
            if (!priv->hwHigh) priv->hwHigh = 0xFF;
            break;
        case epicsUInt16T:
            if (priv->hwHigh > 0xFFFF) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0x0000;
            if (!priv->hwHigh) priv->hwHigh = 0xFFFF;
            break;
        case epicsUInt32T:
            if (!priv->hwHigh) priv->hwLow = 0x00000000;
            if (!priv->hwHigh) priv->hwHigh = 0xFFFFFFFF;
            break;
        case epicsInt8T:
            if (priv->hwHigh > 0x7F) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0xFFFFFF81;
            if (!priv->hwHigh) priv->hwHigh = 0x0000007F;
            break;
        case epicsInt16T:
            if (priv->hwHigh > 0x7FFF) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0xFFFF8001;
            if (!priv->hwHigh) priv->hwHigh = 0x00007FFF;
            break;
        case epicsInt32T:
            if (!priv->hwHigh) priv->hwLow = 0x80000001;
            if (!priv->hwHigh) priv->hwHigh = 0x7FFFFFFF;
            break;
        default:
            if (priv->hwHigh || priv->hwLow) {
                errlogSevPrintf(errlogMinor,
                    "ioParse %s: L or H makes no sense with this data type\n",
                    recordName);
            } 
            break;   
    }
    s7plcDebugLog(1, "ioParse %s: dlen=%d\n",recordName, priv->dlen);
    s7plcDebugLog(1, "ioParse %s: B=%d\n",   recordName, priv->bit);
    s7plcDebugLog(1, "ioParse %s: L=%#x\n",  recordName, priv->hwLow);
    s7plcDebugLog(1, "ioParse %s: H=%#x\n",  recordName, priv->hwHigh);

    if (status)
    {
        errlogSevPrintf(errlogMinor,
            "ioParse %s: L or H out of range for this data type\n",
            recordName);
        return status;
    }
    
    return 0;
}

/* bi for status bit ************************************************/

static long s7plcInitRecordStat(biRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStat: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordStat");
    status = ioParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStat: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    record->dpvt = priv;
    return 0;
}


static long s7plcReadStat(biRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    status = s7plcReadArray(priv->station, 0, 0, 0, NULL);
    if (status == S_drv_noConn)
    {
        record->rval = 0;
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
        return status;
    }
    record->rval = 1;
    return 0;
}

/* bi ***************************************************************/

static long s7plcInitRecordBi(biRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordBi: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordBi");
    status = ioParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordBi: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordBi %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->mask = 1 << priv->bit;
    record->dpvt = priv;
    return 0;
}

static long s7plcReadBi(biRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dlen)
    {
        case epicsInt8T:
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->st_offs,
                1, &rval8);
            s7plcDebugLog(3, "bi %s: read 8bit %02x\n",
                record->name, rval8);
            rval32 = rval8;
            break;
        case epicsInt16T:
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->st_offs,
                2, &rval16);
            s7plcDebugLog(3, "bi %s: read 16bit %04x\n",
                record->name, rval16);
            rval32 = rval16;
            break;
        case epicsInt32T:
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->st_offs,
                4, &rval32);
            s7plcDebugLog(3, "bi %s: read 32bit %04x\n",
                record->name, rval32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    record->rval = rval32 & record->mask;
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* bo ***************************************************************/

static long s7plcInitRecordBo(boRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordBo: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordBo");
    status = ioParse(record->name, record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordBo: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordBo %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->mask = 1 << priv->bit;
    record->dpvt = priv;
    return 0;
}

static long s7plcWriteBo(boRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8, mask8;
    epicsUInt16 rval16, mask16;
    epicsUInt32 rval32, mask32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dlen)
    {
        case epicsInt8T:
        case epicsUInt8T:
            rval8 = record->rval;
            mask8 = record->mask;
            s7plcDebugLog(2, "bo %s: write 8bit %02x mask %02x\n",
                record->name, rval8, mask8);
            status = s7plcWriteMasked(priv->station, priv->st_offs,
                1, &rval8, &mask8);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            rval16 = record->rval;
            mask16 = record->mask;
            s7plcDebugLog(2, "bo %s: write 16bit %04x mask %04x\n",
                record->name, rval16, mask16);
            status = s7plcWriteMasked(priv->station, priv->st_offs,
                2, &rval16, &mask16);
            break;
        case epicsInt32T:
        case epicsUInt32T:
            rval32 = record->rval;
            mask32 = record->mask;
            s7plcDebugLog(2, "bo %s: write 32bit %08x mask %08x\n",
                record->name, rval32, mask32);
            status = s7plcWriteMasked(priv->station, priv->st_offs,
                4, &rval32, &mask32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* mbbi *************************************************************/

static long s7plcInitRecordMbbi(mbbiRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbi: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordMbbi");
    status = ioParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbi: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordMbbi %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    if (record->shft > 0) record->mask <<= record->shft;
    record->dpvt = priv;
    return 0;
}

static long s7plcReadMbbi(mbbiRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->st_offs,
                1, &rval8);
            s7plcDebugLog(3, "mbbi %s: read 8bit %02x\n",
                record->name, rval8);
            rval32 = rval8;
            break;
        case epicsInt16T:
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->st_offs,
                2, &rval16);
            s7plcDebugLog(3, "mbbi %s: read 16bit %04x\n",
                record->name, rval16);
            rval32 = rval16;
            break;
        case epicsInt32T:
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->st_offs,
                4, &rval32);
            s7plcDebugLog(3, "mbbi %s: read 32bit %04x\n",
                record->name, rval32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    record->rval = rval32 & record->mask;
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* mbbo *************************************************************/

static long s7plcInitRecordMbbo(mbboRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbo: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordMbbo");
    status = ioParse(record->name, record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbo: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    if (record->shft > 0) record->mask <<= record->shft;
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordMbbo %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 2;
}

static long s7plcWriteMbbo(mbboRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8, mask8;
    epicsUInt16 rval16, mask16;
    epicsUInt32 rval32, mask32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dlen)
    {
        case epicsInt8T:
        case epicsUInt8T:
            rval8 = record->rval;
            mask8 = record->mask;
            s7plcDebugLog(2, "mbbo %s: write 8bit %02x mask %02x\n",
                record->name, rval8, mask8);
            status = s7plcWriteMasked(priv->station, priv->st_offs,
                1, &rval8, &mask8);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            rval16 = record->rval;
            mask16 = record->mask;
            s7plcDebugLog(2, "mbbo %s: write 16bit %04x mask %04x\n",
                record->name, rval16, mask16);
            status = s7plcWriteMasked(priv->station, priv->st_offs,
                2, &rval16, &mask16);
            break;
        case epicsInt32T:
        case epicsUInt32T:
            rval32 = record->rval;
            mask32 = record->mask;
            s7plcDebugLog(2, "mbbo %s: write 32bit %08x mask %08x\n",
                record->name, rval32, mask32);
            status = s7plcWriteMasked(priv->station, priv->st_offs,
                4, &rval32, &mask32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* mbbiDirect *******************************************************/

static long s7plcInitRecordMbbiDirect(mbbiDirectRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbiDirect: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordMbbiDirect");
    status = ioParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbiDirect: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    if (record->shft > 0) record->mask <<= record->shft;
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordMbbiDirect %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 0;
}

static long s7plcReadMbbiDirect(mbbiDirectRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->st_offs,
                1, &rval8);
            s7plcDebugLog(3, "mbbiDirect %s: read 8bit %02x\n",
                record->name, rval8);
            rval32 = rval8;
            break;
        case epicsInt16T:
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->st_offs,
                2, &rval16);
            s7plcDebugLog(3, "mbbiDirect %s: read 16bit %04x\n",
                record->name, rval16);
            rval32 = rval16;
            break;
        case epicsInt32T:
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->st_offs,
                4, &rval32);
            s7plcDebugLog(3, "mbbiDirect %s: read 32bit %08x\n",
                record->name, rval32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    record->rval = rval32 & record->mask;
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* mbboDirect *******************************************************/

static long s7plcInitRecordMbboDirect(mbboDirectRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbboDirect: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordMbboDirect");
    status = ioParse(record->name, record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbboDirect: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    if (record->shft > 0) record->mask <<= record->shft;
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordMbboDirect %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 0;
}

static long s7plcWriteMbboDirect(mbboDirectRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8, mask8;
    epicsUInt16 rval16, mask16;
    epicsUInt32 rval32, mask32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dlen)
    {
        case epicsInt8T:
        case epicsUInt8T:
            rval8 = record->rval;
            mask8 = record->mask;
            s7plcDebugLog(2, "mbboDirect %s: write 8bit %02x mask %02x\n",
                record->name, rval8, mask8);
            status = s7plcWriteMasked(priv->station, priv->st_offs,
                1, &rval8, &mask8);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            rval16 = record->rval;
            mask16 = record->mask;
            s7plcDebugLog(2, "mbboDirect %s: write 16bit %04x mask %04x\n",
                record->name, rval16, mask16);
            status = s7plcWriteMasked(priv->station, priv->st_offs,
                2, &rval16, &mask16);
            break;
        case epicsInt32T:
        case epicsUInt32T:
            rval32 = record->rval;
            mask32 = record->mask;
            s7plcDebugLog(2, "mbboDirect %s: write 32bit %08x mask %08x\n",
                record->name, rval32, mask32);
            status = s7plcWriteMasked(priv->station, priv->st_offs,
                4, &rval32, &mask32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n", record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* longin ***********************************************************/

static long s7plcInitRecordLongin(longinRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongin: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordLongin");
    status = ioParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongin: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordLongin %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 0;
}

static long s7plcReadLongin(longinRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    signed char sval8;
    epicsUInt8 uval8;
    epicsInt16 sval16;
    epicsUInt16 uval16;
    epicsInt32 sval32;
    epicsUInt32 uval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
            status = s7plcRead(priv->station, priv->st_offs,
                1, &sval8);
            s7plcDebugLog(3, "longin %s: read 8bit %02x\n",
                record->name, sval8);
            record->val = sval8;
            break;
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->st_offs,
                1, &uval8);
            s7plcDebugLog(3, "longin %s: read 8bit %02x\n",
                record->name, uval8);
            record->val = uval8;
            break;
        case epicsInt16T:
            status = s7plcRead(priv->station, priv->st_offs,
                2, &sval16);
            s7plcDebugLog(3, "longin %s: read 16bit %04x\n",
                record->name, sval16);
            record->val = sval16;
            break;
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->st_offs,
                2, &uval16);
            s7plcDebugLog(3, "longin %s: read 16bit %04x\n",
                record->name, sval16);
            record->val = uval16;
            break;
        case epicsInt32T:
            status = s7plcRead(priv->station, priv->st_offs,
                4, &sval32);
            s7plcDebugLog(3, "longin %s: read 32bit %04x\n",
                record->name, sval32);
            record->val = sval32;
            break;
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->st_offs,
                4, &uval32);
            s7plcDebugLog(3, "longin %s: read 32bit %04x\n",
                record->name, uval32);
            record->val = uval32;
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* longout **********************************************************/

static long s7plcInitRecordLongout(longoutRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongout: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordLongout");
    status = ioParse(record->name, record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongout: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordLongout %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 0;
}

static long s7plcWriteLongout(longoutRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dlen)
    {
        case epicsInt8T:
        case epicsUInt8T:
            rval8 = record->val;
            s7plcDebugLog(2, "longout %s: write 8bit %02x\n",
                record->name, rval8);
            status = s7plcWrite(priv->station, priv->st_offs,
                1, &rval8);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            rval16 = record->val;
            s7plcDebugLog(2, "longout %s: write 16bit %04x\n",
                record->name, rval16);
            status = s7plcWrite(priv->station, priv->st_offs,
                2, &rval16);
            break;
        case epicsInt32T:
        case epicsUInt32T:
            rval32 = record->val;
            s7plcDebugLog(2, "longout %s: write 32bit %08x\n",
                record->name, rval32);
            status = s7plcWrite(priv->station, priv->st_offs,
                4, &rval32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* ai ***************************************************************/

static long s7plcInitRecordAi(aiRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordAi: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordAi");
    status = ioParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordAi: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
        case epicsFloat32T:
        case epicsFloat64T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordAi %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    s7plcSpecialLinconvAi(record, TRUE);
    return 0;
}

static long s7plcReadAi(aiRecord *record)
{
    int status, noconvert = FALSE;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    signed char sval8;
    unsigned char uval8;
    epicsInt16 sval16;
    epicsUInt16 uval16;
    epicsInt32 sval32;
    epicsUInt32 uval32;
    epicsFloat32 val32;
    epicsFloat64 val64;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
            status = s7plcRead(priv->station, priv->st_offs,
                1, &sval8);
            s7plcDebugLog(3, "ai %s: read 8bit %02x\n",
                record->name, sval8);
            record->rval = sval8;
            break;
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->st_offs,
                1, &uval8);
            s7plcDebugLog(3, "ai %s: read 8bit %02x\n",
                record->name, uval8);
            record->rval = uval8;
            break;
        case epicsInt16T:
            status = s7plcRead(priv->station, priv->st_offs,
                2, &sval16);
            s7plcDebugLog(3, "ai %s: read 16bit %04x\n",
                record->name, sval16);
            record->rval = sval16;
            break;
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->st_offs,
                2, &uval16);
            s7plcDebugLog(3, "ai %s: read 16bit %04x\n",
                record->name, uval16);
            record->rval = uval16;
            break;
        case epicsInt32T:
            status = s7plcRead(priv->station, priv->st_offs,
                4, &sval32);
            s7plcDebugLog(3, "ai %s: read 32bit %04x\n",
                record->name, sval32);
            record->rval = sval32;
            break;
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->st_offs,
                4, &uval32);
            s7plcDebugLog(3, "ai %s: read 32bit %04x\n",
                record->name, uval32);
            record->rval = uval32;
            break;
        case epicsFloat32T:
            status = s7plcRead(priv->station, priv->st_offs,
                4, &val32);
            s7plcDebugLog(3, "ai %s: read 32bit %04x = %g\n",
                record->name, *(unsigned int*) &val32, val32);
            record->val = val32;
            noconvert = TRUE;
            break;
        case epicsFloat64T:
            status = s7plcRead(priv->station, priv->st_offs,
                8, &val64);
            s7plcDebugLog(3, "ai %s: read 64bit %08Lx = %g\n",
                record->name, *(long long*) &val64, val64);
            record->val = val64;
            noconvert = TRUE;
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
        return status;
    }
    if (noconvert)
    {
        record->udf = FALSE;
        return 2;
    }
    return 0;
}

static long s7plcSpecialLinconvAi(aiRecord *record, int after)
{
    epicsUInt32 hwSpan;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;

    if (after) {
        hwSpan = priv->hwHigh - priv->hwLow;
        record->eslo = (record->eguf - record->egul) / hwSpan;
        record->eoff = (priv->hwHigh*record->egul - priv->hwLow*record->eguf) / hwSpan;;
    }
    return 0;
}

/* ao ***************************************************************/

static long s7plcInitRecordAo(aoRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordAo: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordAo");
    status = ioParse(record->name, record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordAo: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
        case epicsFloat32T:
        case epicsFloat64T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordAo %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    s7plcSpecialLinconvAo(record, TRUE);
    return 0;
}

static long s7plcWriteAo(aoRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;
    epicsFloat32 val32;
    epicsFloat64 val64;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    rval32 = record->rval;
    switch (priv->dtype)
    {
        case epicsInt8T:
            if (record->rval > priv->hwHigh) rval32 = priv->hwHigh;
            if (record->rval < priv->hwLow) rval32 = priv->hwLow;
            rval8 = rval32;
            s7plcDebugLog(2, "ao %s: write 8bit %02x\n",
                record->name, rval8 & 0xff);
            status = s7plcWrite(priv->station, priv->st_offs,
                1, &rval8);
            break;
        case epicsUInt8T:
            if (rval32 > (epicsUInt32)priv->hwHigh) rval32 = priv->hwHigh;
            if (rval32 < (epicsUInt32)priv->hwLow) rval32 = priv->hwLow;
            rval8 = rval32;
            s7plcDebugLog(2, "ao %s: write 8bit %02x\n",
                record->name, rval8 & 0xff);
            status = s7plcWrite(priv->station, priv->st_offs,
                1, &rval8);
            break;
        case epicsInt16T:
            if (record->rval > priv->hwHigh) rval32 = priv->hwHigh;
            if (record->rval < priv->hwLow) rval32 = priv->hwLow;
            rval16 = rval32;
            s7plcDebugLog(2, "ao %s: write 16bit %04x\n",
                record->name, rval16 & 0xffff);
            status = s7plcWrite(priv->station, priv->st_offs,
                2, &rval16);
            break;
        case epicsUInt16T:
            if (rval32 > (epicsUInt32)priv->hwHigh) rval32 = priv->hwHigh;
            if (rval32 < (epicsUInt32)priv->hwLow) rval32 = priv->hwLow;
            rval16 = rval32;
            s7plcDebugLog(2, "ao %s: write 16bit %04x\n",
                record->name, rval16 & 0xffff);
            status = s7plcWrite(priv->station, priv->st_offs,
                2, &rval16);
            break;
        case epicsInt32T:
            if (record->rval > priv->hwHigh) rval32 = priv->hwHigh;
            if (record->rval < priv->hwLow) rval32 = priv->hwLow;
            s7plcDebugLog(2, "ao %s: write 32bit %08x\n",
                record->name, rval32);
            status = s7plcWrite(priv->station, priv->st_offs,
                4, &rval32);
            break;
        case epicsUInt32T:
            if (rval32 > (epicsUInt32)priv->hwHigh) rval32 = priv->hwHigh;
            if (rval32 < (epicsUInt32)priv->hwLow) rval32 = priv->hwLow;
            s7plcDebugLog(2, "ao %s: write 32bit %08x\n",
                record->name, rval32);
            status = s7plcWrite(priv->station, priv->st_offs,
                4, &rval32);
            break;
        case epicsFloat32T:
            val32 = record->val;
            s7plcDebugLog(2, "ao %s: write 32bit %08x\n",
                record->name, *(epicsInt32*)&val32);
            status = s7plcWrite(priv->station, priv->st_offs,
                4, &val32);
            break;
        case epicsFloat64T:
            val64 = record->val;
            s7plcDebugLog(2, "ao %s: write 64bit %016Lx\n",
                record->name, *(long long*)&val64);
            status = s7plcWrite(priv->station, priv->st_offs,
                8, &val64);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

static long s7plcSpecialLinconvAo(aoRecord *record, int after)
{
    epicsUInt32 hwSpan;
    S7memPrivate_t *priv = (S7memPrivate_t *) record->dpvt;

    if (after) {
        hwSpan = priv->hwHigh - priv->hwLow;
        record->eslo = (record->eguf - record->egul) / hwSpan;
        record->eoff = (priv->hwHigh*record->egul - priv->hwLow*record->eguf) / hwSpan;;
    }
    return 0;
}

/* stringin *********************************************************/

static long s7plcInitRecordStringin(stringinRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStringin: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordStringin");
    priv->dtype = epicsStringT;
    priv->dlen = sizeof(record->val);
    status = ioParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongin: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    if (priv->dtype != epicsStringT)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcInitRecordStringin %s: illegal data type\n",
            record->name);
        return S_db_badField;
    }
    if (priv->dlen > sizeof(record->val))
    {
        errlogSevPrintf(errlogMinor, "%s: string size reduced from %d to %d\n",
            record->name, priv->dlen, sizeof(record->val));
        priv->dlen = sizeof(record->val);
    }
    record->dpvt = priv;
    return 0;
}

static long s7plcReadStringin(stringinRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    memset(record->val, 0, priv->dlen);
    status = s7plcReadArray(priv->station, priv->st_offs,
                1, priv->dlen, record->val);
    s7plcDebugLog(3, "stringin %s: read array of %d 8bit values\n",
        record->name, priv->dlen);
    if (record->val[priv->dlen] && !memchr(record->val, 0, priv->dlen))
    {
        /* truncate oversize string */
        record->val[priv->dlen] = 0;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* stringout ********************************************************/

static long s7plcInitRecordStringout(stringoutRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStringout: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordStringout");
    priv->dtype = epicsStringT;
    priv->dlen = sizeof(record->val);
    status = ioParse(record->name, record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStringout: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    if (priv->dtype != epicsStringT)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcInitRecordStringout %s: illegal data type\n",
            record->name);
        return S_db_badField;
    }
    if (priv->dlen > sizeof(record->val))
    {
        errlogSevPrintf(errlogMinor, "%s: string size reduced from %d to %d\n",
            record->name, priv->dlen, sizeof(record->val));
        priv->dlen = sizeof(record->val);
    }
    record->dpvt = priv;
    return 0;
}

static long s7plcWriteStringout(stringoutRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    s7plcDebugLog(2, "stringout %s: write %d 8bit values: \"%.*s\"\n",
        record->name, priv->dlen, priv->dlen, record->val);
    status = s7plcWriteArray(priv->station, priv->st_offs,
        1, priv->dlen, record->val);
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* waveform *********************************************************/

static long s7plcInitRecordWaveform(waveformRecord *record)
{
    S7memPrivate_t *priv;
    int status;
    
    if (record->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordWaveform: illegal INP field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordWaveform");
    switch (record->ftvl)
    {
        case DBF_CHAR:
            priv->dtype = epicsInt8T;
            priv->dlen = 1;
            break;
        case DBF_UCHAR:
            priv->dtype = epicsUInt8T;
            priv->dlen = 1;
            break;
        case DBF_SHORT:
            priv->dtype = epicsInt16T;
            priv->dlen = 2;
            break;
        case DBF_USHORT:
            priv->dtype = epicsUInt16T;
            priv->dlen = 2;
            break;
        case DBF_LONG:
            priv->dtype = epicsInt32T;
            priv->dlen = 4;
            break;
        case DBF_ULONG:
            priv->dtype = epicsUInt32T;
            priv->dlen = 4;
            break;
        case DBF_FLOAT:
            priv->dtype = epicsFloat32T;
            priv->dlen = 4;
            break;
        case DBF_DOUBLE:
            priv->dtype = epicsFloat64T;
            priv->dlen = 8;
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordWaveform %s: illegal FTVL value\n",
                record->name);
            return S_db_badField;
    }
    status = ioParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordWaveform: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    record->nord = record->nelm;
    switch (priv->dtype)
    {
        case S7MEM_TIME:
            if ((record->ftvl != DBF_CHAR) && (record->ftvl != DBF_UCHAR))
            {
                status = S_db_badField;
            }
            if (record->nord > 8) record->nord = 8;
            break;
        case epicsFloat64T:
            if (record->ftvl != DBF_DOUBLE)
            {
                status = S_db_badField;
            }
            break;
        case epicsFloat32T:
            if (record->ftvl != DBF_FLOAT)
            {
                status = S_db_badField;
            }
            break;
        case epicsStringT:    
            if ((record->ftvl == DBF_CHAR) || (record->ftvl == DBF_UCHAR))
            {
                if (!priv->dlen) priv->dlen = record->nelm;
                if (priv->dlen > record->nelm) priv->dlen = record->nelm;
                break;
            }
            break;
        case epicsInt8T:
        case epicsUInt8T:
            if ((record->ftvl != DBF_CHAR) && (record->ftvl == DBF_UCHAR))
            {
                status = S_db_badField;
            }
            break;
        case epicsInt16T:
        case epicsUInt16T:
            if ((record->ftvl != DBF_SHORT) && (record->ftvl == DBF_USHORT))
            {
                status = S_db_badField;
            }
            break;
        case epicsInt32T:
        case epicsUInt32T:
            if ((record->ftvl != DBF_LONG) && (record->ftvl == DBF_ULONG))
            {
                status = S_db_badField;
            }
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordWaveform %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcInitRecordWaveform %s: wrong FTVL field for this data type",
            record->name);
        return status;
    }
    record->dpvt = priv;
    return 0;
}

/*
 * bcd2d routine to convert byte from BCD to decimal format.
 */
static unsigned char bcd2d(unsigned char bcd)
{
    unsigned char tmp;

    tmp = bcd & 0xF;
    tmp += ((bcd >> 4) & 0xF)*10;

    return tmp;
}

static long s7plcReadWaveform(waveformRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    char Time[8];
    int i;
    char *p;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsStringT:
            status = s7plcReadArray(priv->station, priv->st_offs,
                1, record->nelm, record->bptr);
            s7plcDebugLog(3, "waveform %s: read %ld values of 8bit to %p\n",
                record->name, record->nelm, record->bptr);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            status = s7plcReadArray(priv->station, priv->st_offs,
                2, record->nelm, record->bptr);
            s7plcDebugLog(3, "waveform %s: read %ld values of 16bit to %p\n",
                record->name, record->nelm, record->bptr);
            break;
        case epicsInt32T:
        case epicsUInt32T:
        case epicsFloat32T:
            status = s7plcReadArray(priv->station, priv->st_offs,
                4, record->nelm, record->bptr);
            s7plcDebugLog(3, "waveform %s: read %ld values of 32bit to %p\n",
                record->name, record->nelm, record->bptr);
            break;
        case epicsFloat64T:
            status = s7plcReadArray(priv->station, priv->st_offs,
                8, record->nelm, record->bptr);
            s7plcDebugLog(3, "waveform %s: read %ld values of 64bit to %p\n",
                record->name, record->nelm, record->bptr);
            break;
        case S7MEM_TIME:
            status = s7plcReadArray(priv->station, priv->st_offs,
                1, 8, Time);
            s7plcDebugLog(3, "waveform %s: read 8 values of 8bit to %p\n",
                record->name, record->bptr);
            if (status) break;
            for (i = 0, p = record->bptr; i < record->nelm; i++)
                *p++ = (i == 7)? Time[i] : bcd2d(Time[i]);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal, "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    record->nord = record->nelm;
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}
