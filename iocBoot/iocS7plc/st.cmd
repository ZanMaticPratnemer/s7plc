#!../../bin/linux-x86_64/S7plc

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/s7plc.dbd"
s7plc_registerRecordDeviceDriver pdbbase

#var s7plcDebug <level>
#level=-1:  no output
#level=0:   errors only
#level=1:   startup messages
#level=2: + output record processing
#level=3: + inputput record processing
#level=4: + driver calls
#level=5: + io printout
#be careful using level>1 since many messages may introduce delays

var s7plcDebug 0

#s7plcConfigure name,IPaddr,port,inSize,outSize,bigEndian,recvTimeout,sendIntervall
#connects to PLC <name> on address <IPaddr> port <port>
#<inSize>        : size of data bock PLC -> IOC [bytes]
#<outSize>       : size of data bock IOC -> PLC [bytes]
#<bigEndian>=1   : motorola format data (MSB first)
#<bigEndian>=0   : intel format data (LSB first)
#<recvTimeout>   : time to wait for input before disconnecting [ms]
#<sendIntervall> : time to wait before sending new data to PLC [ms]

s7plcConfigure Testsystem0,127.0.0.1,2000,96,112,1,2000,100

dbLoadRecords "db/example.db"

# Uncomment for int64 records
#dbLoadRecords "db/example_int64.db"

cd "${TOP}/iocBoot/${IOC}"
iocInit
