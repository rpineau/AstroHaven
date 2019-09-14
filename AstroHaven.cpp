//
//  AstroHaven.cpp
//  CAstroHaven
//
//  Created by Rodolphe Pineau on 2017-4-6
//  AstroHaven X2 plugin

#include "AstroHaven.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

CAstroHaven::CAstroHaven()
{
    // set some sane values
    m_pSerx = NULL;
    m_bIsConnected = false;

    m_dCurrentAzPosition = 0.0;
    m_dCurrentElPosition = 0.0;

    m_nShutterState = UNKNOWN;
	m_nCurrentShutterAction = UNKNOWN;


#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\X2_AstroHaven.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_AstroHaven.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_AstroHaven.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::CAstroHaven] Version 2019_09_13_1930.\n", timestamp);
    fprintf(Logfile, "[%s] [CAstroHaven::CAstroHaven] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif

}

CAstroHaven::~CAstroHaven()
{

}

int CAstroHaven::Connect(const char *pszPort)
{
    int nErr = PluginOK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] Connecting to %s.\n", timestamp, pszPort);
    fflush(Logfile);
#endif
    
    // 9600 8N1
    if(m_pSerx->open(pszPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    m_pSleeper->sleep(2000);
    m_pSerx->purgeTxRx();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] Connected to %s.\n", timestamp, pszPort);
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] Getting shutter state.\n", timestamp);
    fflush(Logfile);
#endif

    // For e close the shutter
	/*
    nErr = setShutterStateToClosed();
    if(nErr) {
        return ERR_COMMNOLINK;
    }
	*/
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] AstroHaven init done\n", timestamp);
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] m_bIsConnected = %s.\n", timestamp, m_bIsConnected?"True":"False");
    fflush(Logfile);
#endif

    syncDome(m_dCurrentAzPosition,m_dCurrentElPosition);

    return nErr;
}


void CAstroHaven::Disconnect()
{
    if(m_bIsConnected) {
        m_pSerx->purgeTxRx();
        m_pSerx->close();
    }
    m_bIsConnected = false;
}


int CAstroHaven::readResponse(char *pszRespBuffer, unsigned int nBufferLen, unsigned long nExpectRespLen, int TimeOut)
{
    int nErr = PluginOK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
	int nRetries = 0;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, nExpectRespLen, ulBytesRead, TimeOut);
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CAstroHaven::readResponse] readFile error %d\n", timestamp, nErr);
            fflush(Logfile);
#endif
            return nErr;
        }

        if (!ulBytesRead) {// timeout
			if(nRetries >= 2) {
				if(ulTotalBytesRead)
					nErr = PluginOK;
				else
					nErr = NO_DATA_TIMEOUT;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				fprintf(Logfile, "[%s] [CAstroHaven::readResponse] Timeout, ulTotalBytesRead = %lu\n", timestamp, ulTotalBytesRead);
				fprintf(Logfile, "[%s] [CAstroHaven::readResponse] Timeout, nErr = %d\n", timestamp, nErr);
				fflush(Logfile);
#endif
				break;
			}
			nRetries++;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				fprintf(Logfile, "[%s] [CAstroHaven::readResponse] Timeout, retring\n", timestamp);
				fprintf(Logfile, "[%s] [CAstroHaven::readResponse] Timeout, nRetries = %d\n", timestamp, nRetries);
				fflush(Logfile);
#endif
		}
		else
			nRetries = 0;

		pszBufPtr += ulBytesRead;
        ulTotalBytesRead += ulBytesRead;
    } while (ulTotalBytesRead < nExpectRespLen && ulTotalBytesRead < nBufferLen );

    return nErr;
}


int CAstroHaven::domeCommand(const char *pszCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAstroHaven::domeCommand] Sending %s\n", timestamp, pszCmd);
    fflush(Logfile);
#endif
    m_pSerx->purgeTxRx();
    nErr = m_pSerx->writeFile((void *)pszCmd, strlen(pszCmd), nBytesWrite);
    m_pSerx->flushTx();
    if(nErr)
        return nErr;

	nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
	if(nErr && nErr != NO_DATA_TIMEOUT)
		return nErr;

	if(strlen(szResp)) {
		strncpy(pszResult, szResp, nResultMaxLen);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CAstroHaven::domeCommand] response : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
    }
	else
		memset(pszResult, 0, nResultMaxLen);

    return PluginOK;
}


int CAstroHaven::setShutterStateToClosed()
{
    int nErr = PluginOK;
    bool bComplete = false;
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    // send a close to make sure we start in the closed state, it should respond with AX and BY
    nErr = closeShutter();
    while (!bComplete) {
        m_pSleeper->sleep(1000);
        isCloseComplete(bComplete);
    }
        
    m_nShutterState = CLOSED;
    
    return nErr;
}




int CAstroHaven::syncDome(double dAz, double dEl)
{

	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
        m_pSerx->purgeTxRx();
	}

    m_dCurrentAzPosition = dAz;
    m_dCurrentElPosition = dEl;
    
    return PluginOK;
}

int CAstroHaven::parkDome()
{
    int nErr = PluginOK;

	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
        m_pSerx->purgeTxRx();
	}
    return nErr;

}

int CAstroHaven::unparkDome()
{
	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
        m_pSerx->purgeTxRx();
	}
    syncDome(m_dCurrentAzPosition,m_dCurrentElPosition);
    return 0;
}

int CAstroHaven::gotoAzimuth(double dNewAz)
{
    int nErr = PluginOK;


	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
        m_pSerx->purgeTxRx();
	}

    m_dCurrentAzPosition = dNewAz;
    if(m_nShutterState == OPEN)
        m_dCurrentElPosition = 90.0;
    else
        m_dCurrentElPosition = 0.0;

    return nErr;
}

int CAstroHaven::openShutter()
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::openShutter] Opening shutter\n", timestamp);
	fflush(Logfile);
#endif

    nErr = domeCommand("a", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::openShutter] Opening shutter response : %s\n", timestamp, szResp);
	fflush(Logfile);
#endif

    m_nCurrentShutterAction = OPENING_A;
	m_nASideState = OPENING;
	m_nBSideState = OPENING;
    // we we already open ?
    if(strstr(szResp, "x")) {
        m_nASideState = OPEN;
		m_nCurrentShutterAction = OPENING_B;
        // if we need to sequence the shutter, we start opening the other side here.
		nErr = domeCommand("b", szResp, SERIAL_BUFFER_SIZE);
		if(nErr)
			return nErr;
		if(strstr(szResp, "y")) {
			m_nBSideState = OPEN;
			m_nCurrentShutterAction = OPEN;
		}
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::openShutter] states -> m_nASideState = %d, m_nBSideState : %d\n", timestamp, m_nASideState, m_nBSideState);
	fflush(Logfile);
#endif

    return nErr;
}

int CAstroHaven::closeShutter()
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::closeShutter] Closing shutter\n", timestamp);
	fflush(Logfile);
#endif

	// close B side first
	nErr = domeCommand("B", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::openShutter] Closing shutter response : %s\n", timestamp, szResp);
	fflush(Logfile);
#endif

	m_nCurrentShutterAction = CLOSING_B;
	m_nASideState = CLOSING;
    m_nBSideState = CLOSING;
    // we we already closed ?
    if(strstr(szResp, "Y")) {
        m_nASideState = CLOSED;
		m_nCurrentShutterAction = CLOSING_A;
        // if we need to sequence the shutter, we start closing the other side here.
		nErr = domeCommand("A", szResp, SERIAL_BUFFER_SIZE);
		if(nErr)
			return nErr;
		if(strstr(szResp, "X")) {
			m_nBSideState = CLOSED;
			m_nCurrentShutterAction = CLOSED;
		}
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::openShutter] states -> m_nASideState = %d, m_nBSideState : %d\n", timestamp, m_nASideState, m_nBSideState);
	fflush(Logfile);
#endif

	return nErr;
}

int CAstroHaven::findHome()
{
	int nErr = PluginOK;

	if(!m_bIsConnected)
		return NOT_CONNECTED;

	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
		m_pSerx->purgeTxRx();
	}
	return nErr;
}

int CAstroHaven::isGoToComplete(bool &bComplete)
{
    int nErr = PluginOK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;
    bComplete = true;

    return nErr;
}

int CAstroHaven::isOpenComplete(bool &bComplete)
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete] In States :  m_nCurrentShutterAction = %d\n", timestamp, m_nCurrentShutterAction);
	fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete]              m_nASideState = %d\n", timestamp, m_nASideState);
	fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete]              m_nBSideState : %d\n", timestamp, m_nBSideState);
	fflush(Logfile);
#endif

	if(m_nCurrentShutterAction == OPEN) {
        bComplete = true;
        return nErr;
    }

    bComplete = false;

	switch(m_nCurrentShutterAction) {
		case OPENING_A:
			nErr = domeCommand("a", szResp, SERIAL_BUFFER_SIZE);
			break;
		case OPENING_B:
			nErr = domeCommand("b", szResp, SERIAL_BUFFER_SIZE);
			break;
		default:
			break;
	}

	if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete] domeCommand error = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
	if(!strlen(szResp))  {//no response.
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete] No response\n", timestamp);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;	// PluginOK we ignore the timeouts //  return ERR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete] response : '%s'\n", timestamp, szResp);
	fflush(Logfile);
#endif

    if(strstr(szResp, "x")) {
        m_nASideState = OPEN;
    }
    if(strstr(szResp, "y")) {
        m_nBSideState = OPEN;
    }

	if ( m_nCurrentShutterAction == OPENING_A && m_nASideState == OPEN) {
		// now open B side
		nErr = domeCommand("b", szResp, SERIAL_BUFFER_SIZE);
		if(nErr)
			return nErr;
		m_nCurrentShutterAction = OPENING_B;
	}
	else if ( m_nCurrentShutterAction == OPENING_B && m_nBSideState == OPEN) {
		m_nCurrentShutterAction = OPEN;
		bComplete = true;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete] Out States : m_nCurrentShutterAction = %d\n", timestamp, m_nCurrentShutterAction);
	fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete]              m_nASideState = %d\n", timestamp, m_nASideState);
	fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete]              m_nBSideState : %d\n", timestamp, m_nBSideState);
	fprintf(Logfile, "[%s] [CAstroHaven::isOpenComplete] bComplete = %s\n", timestamp, bComplete?"True":"False");
	fflush(Logfile);
#endif

    return nErr;
}

int CAstroHaven::isCloseComplete(bool &bComplete)
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete] In States :  m_nCurrentShutterAction = %d\n", timestamp, m_nCurrentShutterAction);
	fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete]              m_nASideState = %d\n", timestamp, m_nASideState);
	fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete]              m_nBSideState : %d\n", timestamp, m_nBSideState);
	fflush(Logfile);
#endif

    bComplete = false;
	 switch(m_nCurrentShutterAction) {
		 case CLOSING_B:
			 nErr = domeCommand("B", szResp, SERIAL_BUFFER_SIZE);
			 break;
		 case CLOSING_A:
			 nErr = domeCommand("A", szResp, SERIAL_BUFFER_SIZE);
			 break;
		 default:
			 break;
	 }

	if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete] domeCommand error = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    if(!strlen(szResp))  {//no response.
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete] No response\n", timestamp);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;	// PluginOK ignore timeouts //  return ERR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete] response : '%s'\n", timestamp, szResp);
	fflush(Logfile);
#endif

    if(strstr(szResp, "X")) {
        m_nASideState = CLOSED;
        // if we need to sequence the shutter, we start closing the other side here.
    }
    if(strstr(szResp, "Y")) {
        m_nBSideState = CLOSED;
    }

	if ( m_nCurrentShutterAction == CLOSING_B && m_nBSideState == CLOSED) {
		// now close A side
		nErr = domeCommand("A", szResp, SERIAL_BUFFER_SIZE);
		if(nErr)
			return nErr;
		m_nCurrentShutterAction = CLOSING_A;
	}
	else if ( m_nCurrentShutterAction == CLOSING_A && m_nASideState == CLOSED) {
		m_nCurrentShutterAction = CLOSED;
		bComplete = true;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete] Out States : m_nCurrentShutterAction = %d\n", timestamp, m_nCurrentShutterAction);
	fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete]              m_nASideState = %d\n", timestamp, m_nASideState);
	fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete]              m_nBSideState : %d\n", timestamp, m_nBSideState);
	fprintf(Logfile, "[%s] [CAstroHaven::isCloseComplete] bComplete = %s\n", timestamp, bComplete?"True":"False");
	fflush(Logfile);
#endif

	return nErr;
}


int CAstroHaven::isParkComplete(bool &bComplete)
{
    int nErr = PluginOK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
        m_pSerx->purgeTxRx();
	}
    bComplete = true;
    return nErr;
}

int CAstroHaven::isUnparkComplete(bool &bComplete)
{
    int nErr = PluginOK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
        m_pSerx->purgeTxRx();
	}

    bComplete = true;

    return nErr;
}

int CAstroHaven::isFindHomeComplete(bool &bComplete)
{
    int nErr = PluginOK;

	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
        m_pSerx->purgeTxRx();
	}

    bComplete = true;

    return nErr;

}


#pragma mark - Getter / Setter


double CAstroHaven::getCurrentAz()
{
	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
        m_pSerx->purgeTxRx();
	}

    return m_dCurrentAzPosition;
}

double CAstroHaven::getCurrentEl()
{
	if(m_bIsConnected && (m_nCurrentShutterAction == OPEN || m_nCurrentShutterAction == CLOSED || m_nCurrentShutterAction == UNKNOWN)) {
        m_pSerx->purgeTxRx();
	}

    return m_dCurrentElPosition;
}
