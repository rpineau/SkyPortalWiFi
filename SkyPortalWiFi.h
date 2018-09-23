#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

// C++ includes
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/serxinterface.h"
// #include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/mountdriverinterface.h"
#include "../../licensedinterfaces/mount/asymmetricalequatorialinterface.h"

#include "StopWatch.h"


#define SKYPORTAL_DEBUG 2   // define this to have log files, 1 = bad stuff only, 2 and up.. full debug

enum SkyPortalWiFiErrors {SKYPORTAL_OK=0, NOT_CONNECTED, SKYPORTAL_CANT_CONNECT, SKYPORTAL_BAD_CMD_RESPONSE, COMMAND_FAILED, SKYPORTAL_ERROR};

#define SERIAL_BUFFER_SIZE 1024
#define MAX_TIMEOUT 1000
#define LOG_BUFFER_SIZE 1024

#define SOM     0x3B
#define PC      0x20    // we're sending from AUX address 0x20
#define AZM     0x10
#define ALT     0x11
#define GPS     0xb0

#define MSG_LEN 1
#define SRC_DEV 2
#define DST_DEV 3
#define CMD_ID  4


#define SkyPortalWiFi_NB_SLEW_SPEEDS 10
#define SkyPortalWiFi_SLEW_NAME_LENGHT 12

#define STEPS_PER_REVOLUTION    16777216
#define STEPS_PER_DEGREE        STEPS_PER_REVOLUTION / 360.0
#define TRACK_SCALE             60000 / STEPS_PER_DEGREE

enum MC_Commands
{
    MC_GET_POSITION         = 0x01,
    MC_GOTO_FAST            = 0x02,
    MC_SET_POSITION         = 0x04,
    MC_SET_POS_GUIDERATE    = 0x06,
    MC_SET_NEG_GUIDERATE    = 0x07,
    MC_LEVEL_START          = 0x0b,
    MC_PEC_RECORD_START     = 0x0c,
    MC_PEC_PLAYBACK         = 0x0d,
    MC_SET_POS_BACKLASH     = 0x10,
    MC_SET_NEG_BACKLASH     = 0x11,
    MC_LEVEL_DONE           = 0x12,
    MC_SLEW_DONE            = 0x13,
    MC_UNKNOWN              = 0x14,
    MC_PEC_RECORD_DONE      = 0x15,
    MC_PEC_RECORD_STOP      = 0x16,
    MC_GOTO_SLOW            = 0x17,
    MC_AT_INDEX             = 0x18,
    MC_SEEK_INDEX           = 0x19,
    MC_MOVE_POS             = 0x24,
    MC_MOVE_NEG             = 0x25,
    MC_ENABLE_CORDWRAP      = 0x38,
    MC_DISABLE_CORDWRAP     = 0x39,
    MC_SET_CORDWRAP_POS     = 0x3a,
    MC_POLL_CORDWRAP        = 0x3b,
    MC_GET_CORDWRAP_POS     = 0x3c,
    MC_GET_POS_BACKLASH     = 0x40,
    MC_GET_NEG_BACKLASH     = 0x41,
    MC_SET_AUTOGUIDE_RATE   = 0x46,
    MC_GET_AUTOGUIDE_RATE   = 0x47,
    MC_PROGRAM_ENTER        = 0x81,
    MC_PROGRAM_INIT         = 0x82,
    MC_PROGRAM_DATA         = 0x83,
    MC_PROGRAM_END          = 0x84,
    MC_GET_APPRAOCH         = 0xfc,
    MC_sET_APPRAOCH         = 0xfd,
    MC_GET_VER              = 0xfe,
};

enum GPS_Commands
{
    GPS_GET_LAT             = 0x01,
    GPS_GET_LONG            = 0x02,
    GPS_GET_DATE            = 0x03,
    GPS_GET_YEAR            = 0x04,
    GPS_GET_SAT_INFO        = 0x07,
    GPS_GET_RCVR_STATUS     = 0x08,
    GPS_GET_TIME            = 0x33,
    GPS_TIME_VALID          = 0x36,
    GPS_LINKED              = 0x37,
    GPS_GET_HW_VER          = 0x55,
    GPS_GET_COMPASS         = 0xa0,
    GPS_GET_VER             = 0xfe,
};

// Define Class for SkyPortalWiFi controller.
class SkyPortalWiFi
{
public:
	SkyPortalWiFi();
	~SkyPortalWiFi();
	
	int Connect(char *pszPort);
	int Disconnect();
	bool isConnected() const { return m_bIsConnected; }

    void setSerxPointer(SerXInterface *p) { m_pSerx = p; }
    void setTSX(TheSkyXFacadeForDriversInterface *pTSX) { m_pTsx = pTSX; m_dLatitude = m_pTsx->latitude(); }
    void setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper;}

    void setMountMode(bool bIsGEM) { m_bMountIsGEM = bIsGEM;}
    bool isMountGem() { return m_bMountIsGEM;}

    int getNbSlewRates();
    int getRateName(int nZeroBasedIndex, char *pszOut, unsigned int nOutMaxSize);

    int getFirmwareVersion(char *version, unsigned int strMaxLen);

    int getRaAndDec(double &dRa, double &dDec);
    int syncTo(double dRa, double dDec);
    int isAligned(bool &bAligned);

    int setTrackingRates(bool bTrackingOn, bool bIgnoreRates, double dTrackRaArcSecPerMin, double dTrackDecArcSecPerMin);
    int getTrackRates(bool &bTrackingOn, double &dTrackRaArcSecPerMin, double &dTrackDecArcSecPerMin);

    int startSlewTo(double dRa, double dDec, bool bFast = true);
    int isSlewToComplete(bool &bComplete);

    int startOpenSlew(const MountDriverInterface::MoveDir Dir, unsigned int nRate);
    int stopOpenLoopMove();

    int gotoPark(double dRa, double dDEc);
    int getAtPark(bool &bParked);
    int unPark();
    void setParkPosition(double dRa, double dDec);

    int getLimits(double &dHoursEast, double &dHoursWest);

    int Abort();

    MountTypeInterface::Type mountType();
private:

    SerXInterface                       *m_pSerx;
    TheSkyXFacadeForDriversInterface    *m_pTsx;
    SleeperInterface                    *m_pSleeper;

    bool    m_bDebugLog;
    char    m_szLogBuffer[LOG_BUFFER_SIZE];

	bool    m_bIsConnected;
    bool    m_bIsSynced;

    char    m_szFirmwareVersion[SERIAL_BUFFER_SIZE];

    char    m_szHardwareModel[SERIAL_BUFFER_SIZE];
    char    m_szTime[SERIAL_BUFFER_SIZE];
    char    m_szDate[SERIAL_BUFFER_SIZE];
    int     m_nSiteNumber;

    bool    m_bMountIsGEM;
    double  m_dLatitude;

    double  m_dCurrentRa;        // Current mount Ra
    double  m_dCurrentDec;       // Current mount Dec

    int     m_nPrevAltSteps;
    int     m_nPrevAzSteps;

	double  m_dGotoRATarget;    // GoTo Target RA;
	double  m_dGotoDECTarget;   // Goto Target Dec;
	
    bool    m_bJNOW;
    bool    m_b24h;
    bool    m_bDdMmYy;
    bool    m_bTimeSetOnce;
    MountDriverInterface::MoveDir      m_nOpenLoopDir;

    // limits don't change mid-course so we cache them
    bool    m_bLimitCached;
    double  m_dHoursEast;
    double  m_dHoursWest;
    
    int     SkyPortalWiFiSendCommand(const unsigned char *pszCmd, unsigned char *pszResult, int nResultMaxLen);
    int     SkyPortalWiFiReadResponse(unsigned char *pszRespBuffer, int nBufferLen);
    unsigned char checksum(const unsigned char *cMessage, int nLen);
    void    hexdump(const unsigned char* pszInputBuffer, unsigned char *pszOutputBuffer, int nInputBufferSize, int nOutpuBufferSize);

    int     getPosition(int &nAltSteps, int &nAzSteps);
    int     setPosition(int nAltSteps, int nAzSteps);

    int     moveAlt(int nSteps);
    int     moveAz(int nSteps);

    int     setTrackingRatesSteps(int nAltRate, int nAzRate, int nDataLen);

    void    convertDecDegToDDMMSS(double dDeg, char *szResult, char &cSign, unsigned int size);
    int     convertDDMMSSToDecDeg(const char *szStrDeg, double &dDecDeg);
    
    void    convertRaToHHMMSSt(double dRa, char *szResult, unsigned int size);
    int     convertHHMMSStToRa(const char *szStrRa, double &dRa);

    void    stepsToDeg(int nSteps, double &dDeg);
    void    degToSteps(double dDeg, int &nSteps);

    void    fixAltStepsAzSteps(int &nAltSteps, int &AzSteps);
    int     getFixedAlt(int nAltSteps);
    int     getFixedAz(int nAzSteps);

    const char m_aszSlewRateNames[SkyPortalWiFi_NB_SLEW_SPEEDS][SkyPortalWiFi_SLEW_NAME_LENGHT] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    int   parseFields(const char *pszIn, std::vector<std::string> &svFields, char cSeparator);

    CStopWatch      m_Timer;
    CStopWatch      m_trakingTimer;
    bool            m_bIsTracking;
    bool            m_bSiderealOn;
    bool            m_bIsParking;
    bool            m_bGotoFast;

    double          m_dParkRa;
    double          m_dParkDec;


#ifdef SKYPORTAL_DEBUG
    std::string m_sLogfilePath;
	// timestamp for logs
    char *timestamp;
	time_t ltime;
	FILE *Logfile;	  // LogFile
#endif
	
};


