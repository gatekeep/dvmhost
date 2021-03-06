/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMCal project. (https://github.com/g4klx/MMDVMCal)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017,2018 by Andy Uribe CA6JAU
*   Copyright (C) 2017-2021 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "host/calibrate/HostCal.h"
#include "dmr/DMRDefines.h"
#include "modem/port/UARTPort.h"
#include "p25/P25Defines.h"
#include "p25/data/DataHeader.h"
#include "p25/lc/LC.h"
#include "p25/P25Utils.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace modem;

#include <cstdio>
#include <algorithm>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define DMR_CAL_STR         "[Tx] DMR 1200 Hz Tone Mode (2.75Khz Deviation)"
#define P25_CAL_STR         "[Tx] P25 1200 Hz Tone Mode (2.83Khz Deviation)"
#define DMR_LF_CAL_STR      "[Tx] DMR Low Frequency Mode (80 Hz square wave)"
#define P25_LF_CAL_STR      "[Tx] P25 Low Frequency Mode (80 Hz square wave)"
#define DMR_CAL_1K_STR      "[Tx] DMR BS 1031 Hz Test Pattern (TS2 CC1 ID1 TG9)"
#define DMR_DMO_CAL_1K_STR  "[Tx] DMR MS 1031 Hz Test Pattern (TS2 CC1 ID1 TG9)"
#define P25_CAL_1K_STR      "[Tx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1)"
#define DMR_FEC_STR         "[Rx] DMR MS FEC BER Test Mode"
#define DMR_FEC_1K_STR      "[Rx] DMR MS 1031 Hz Test Pattern (CC1 ID1 TG9)"
#define P25_FEC_STR         "[Rx] P25 FEC BER Test Mode"
#define P25_FEC_1K_STR      "[Rx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1)"
#define RSSI_CAL_STR        "RSSI Calibration Mode"

// Voice LC MS Header, CC: 1, srcID: 1, dstID: TG9
const uint8_t VH_DMO1K[] = {
        0x00U, 0x20U, 0x08U, 0x08U, 0x02U, 0x38U, 0x15U, 0x00U, 0x2CU, 0xA0U, 0x14U,
        0x60U, 0x84U, 0x6DU, 0x5DU, 0x7FU, 0x77U, 0xFDU, 0x75U, 0x7EU, 0x30U, 0x30U,
        0x01U, 0x10U, 0x01U, 0x40U, 0x03U, 0xC0U, 0x13U, 0xC1U, 0x1EU, 0x80U, 0x6FU };

// Voice Term MS with LC, CC: 1, srcID: 1, dstID: TG9
const uint8_t VT_DMO1K[] = {
        0x00U, 0x4FU, 0x08U, 0xDCU, 0x02U, 0x88U, 0x15U, 0x78U, 0x2CU, 0xD0U, 0x14U,
        0xC0U, 0x84U, 0xADU, 0x5DU, 0x7FU, 0x77U, 0xFDU, 0x75U, 0x79U, 0x65U, 0x24U,
        0x02U, 0x28U, 0x06U, 0x20U, 0x0FU, 0x80U, 0x1BU, 0xC1U, 0x07U, 0x80U, 0x5CU };

// Voice coding data + FEC, 1031 Hz Test Pattern
const uint8_t VOICE_1K[] = {
        0xCEU, 0xA8U, 0xFEU, 0x83U, 0xACU, 0xC4U, 0x58U, 0x20U, 0x0AU, 0xCEU, 0xA8U,
        0xFEU, 0x83U, 0xA0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x0CU, 0xC4U, 0x58U,
        0x20U, 0x0AU, 0xCEU, 0xA8U, 0xFEU, 0x83U, 0xACU, 0xC4U, 0x58U, 0x20U, 0x0AU };

// Recommended 1011 Hz test pattern for P25 Phase 1 (ANSI/TIA-102.CAAA)
// NAC: 0x293, srcID: 1, dstID: TG1
unsigned char LDU1_1K[] = {
    0x55, 0x75, 0xF5, 0xFF, 0x77, 0xFF, 0x29, 0x35, 0x54, 0x7B, 0xCB, 0x19, 0x4D, 0x0D, 0xCE, 0x24, 0xA1, 0x24,
    0x0D, 0x43, 0x3C, 0x0B, 0xE1, 0xB9, 0x18, 0x44, 0xFC, 0xC1, 0x62, 0x96, 0x27, 0x60, 0xE4, 0xE2, 0x4A, 0x10,
    0x90, 0xD4, 0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4C, 0xFC, 0x16, 0x29, 0x62, 0x76, 0x0E, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x02, 0xF8, 0x6E, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x94,
    0x89, 0xD8, 0x39, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x38, 0x24, 0xA1, 0x24, 0x35, 0x0C, 0xF0, 0x2F, 0x86, 0xE4,
    0x18, 0x44, 0xFF, 0x05, 0x8A, 0x58, 0x9D, 0x83, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x70, 0xE2, 0x4A, 0x12, 0x40,
    0xD4, 0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4F, 0xF0, 0x16, 0x29, 0x62, 0x76, 0x0E, 0x6D, 0xE5, 0xD5, 0x48,
    0xAD, 0xE3, 0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x08, 0xF8, 0x6E, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x96, 0x24,
    0xD8, 0x3B, 0xA1, 0x41, 0xC2, 0xD2, 0xBA, 0x38, 0x90, 0xA1, 0x24, 0x35, 0x0C, 0xF0, 0x2F, 0x86, 0xE4, 0x60,
    0x44, 0xFF, 0x05, 0x8A, 0x58, 0x9D, 0x83, 0x94, 0xC8, 0xFB, 0x02, 0x35, 0xA4, 0xE2, 0x4A, 0x12, 0x43, 0x50,
    0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4F, 0xF0, 0x58, 0x29, 0x62, 0x76, 0x0E, 0xC0, 0x00, 0x00, 0x00, 0x0C,
    0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x0B, 0xE1, 0xB8, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x96, 0x27, 0x60, 0xE4 };

unsigned char LDU2_1K[] = {
    0x55, 0x75, 0xF5, 0xFF, 0x77, 0xFF, 0x29, 0x3A, 0xB8, 0xA4, 0xEF, 0xB0, 0x9A, 0x8A, 0xCE, 0x24, 0xA1, 0x24,
    0x0D, 0x43, 0x3C, 0x0B, 0xE1, 0xB9, 0x18, 0x44, 0xFC, 0xC1, 0x62, 0x96, 0x27, 0x60, 0xEC, 0xE2, 0x4A, 0x10,
    0x90, 0xD4, 0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4C, 0xFC, 0x16, 0x29, 0x62, 0x76, 0x0E, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x02, 0xF8, 0x6E, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x94,
    0x89, 0xD8, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x24, 0xA1, 0x24, 0x35, 0x0C, 0xF0, 0x2F, 0x86, 0xE4,
    0x18, 0x44, 0xFF, 0x05, 0x8A, 0x58, 0x9D, 0x83, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE2, 0x4A, 0x12, 0x40,
    0xD4, 0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4F, 0xF0, 0x16, 0x29, 0x62, 0x76, 0x0E, 0xE0, 0xE0, 0x00, 0x00,
    0x00, 0x03, 0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x08, 0xF8, 0x6E, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x96, 0x24,
    0xD8, 0x39, 0xAE, 0x8B, 0x48, 0xB6, 0x49, 0x38, 0x90, 0xA1, 0x24, 0x35, 0x0C, 0xF0, 0x2F, 0x86, 0xE4, 0x60,
    0x44, 0xFF, 0x05, 0x8A, 0x58, 0x9D, 0x83, 0xB9, 0xA8, 0xF4, 0xF1, 0xFD, 0x60, 0xE2, 0x4A, 0x12, 0x43, 0x50,
    0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4F, 0xF0, 0x58, 0x29, 0x62, 0x76, 0x0E, 0x40, 0x00, 0x00, 0x00, 0x0C,
    0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x0B, 0xE1, 0xB8, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x96, 0x27, 0x60, 0xEC };

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the HostCal class.
/// </summary>
/// <param name="confFile">Full-path to the configuration file.</param>
HostCal::HostCal(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_modem(NULL),
    m_console(),
    m_fec(),
    m_transmit(false),
    m_duplex(true),
    m_txInvert(false),
    m_rxInvert(false),
    m_pttInvert(false),
    m_dcBlocker(true),
    m_txLevel(50.0F),
    m_rxLevel(50.0F),
    m_dmrEnabled(false),
    m_dmrRx1K(false),
    m_p25Enabled(false),
    m_p25Rx1K(false),
    m_txDCOffset(0),
    m_rxDCOffset(0),
    m_dmrSymLevel3Adj(0),
    m_dmrSymLevel1Adj(0),
    m_p25SymLevel3Adj(0),
    m_p25SymLevel1Adj(0),
    m_fdmaPreamble(80U),
    m_dmrRxDelay(7U),
    m_p25CorrCount(5U),
    m_debug(false),
    m_mode(STATE_DMR_CAL),
    m_modeStr(DMR_CAL_STR),
    m_berFrames(0U),
    m_berBits(0U),
    m_berErrs(0U),
    m_berUndecodableLC(0U),
    m_berUncorrectable(0U),
    m_timeout(300U),
    m_timer(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the HostCal class.
/// </summary>
HostCal::~HostCal()
{
    delete m_modem;
}

/// <summary>
/// Executes the calibration processing loop.
/// </summary>
/// <returns>Zero if successful, otherwise error occurred.</returns>
int HostCal::run()
{
    bool ret = yaml::Parse(m_conf, m_confFile.c_str());
    if (!ret) {
        ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
    }

    // initialize system logging
    ret = ::LogInitialise("", "", 0U, 1U);
    if (!ret) {
        ::fprintf(stderr, "unable to open the log file\n");
        return 1;
    }

    getHostVersion();
    ::LogInfo(">> Modem Calibration");

    LogInfo("General Parameters");

    yaml::Node systemConf = m_conf["system"];
    std::string identity = systemConf["identity"].as<std::string>();
    ::LogInfo("    Identity: %s", identity.c_str());

    yaml::Node modemConf = systemConf["modem"];

    yaml::Node modemProtocol = modemConf["protocol"];
    std::string portType = modemProtocol["type"].as<std::string>("null");

    yaml::Node uartProtocol = modemProtocol["uart"];
    std::string uartPort = uartProtocol["port"].as<std::string>();
    uint32_t uartSpeed = uartProtocol["speed"].as<uint32_t>(115200);

    port::IModemPort* modemPort = NULL;
    std::transform(portType.begin(), portType.end(), portType.begin(), ::tolower);
    if (portType == NULL_PORT) {
        ::LogError(LOG_HOST, "Calibration mode is unsupported with the null modem!");
        return 2;
    }
    else if (portType == UART_PORT) {
        port::SERIAL_SPEED serialSpeed = port::SERIAL_115200;
        switch (uartSpeed) {
        case 1200:
            serialSpeed = port::SERIAL_1200;
            break;
        case 2400:
            serialSpeed = port::SERIAL_2400;
            break;
        case 4800:
            serialSpeed = port::SERIAL_4800;
            break;
        case 9600:
            serialSpeed = port::SERIAL_9600;
            break;
        case 19200:
            serialSpeed = port::SERIAL_19200;
            break;
        case 38400:
            serialSpeed = port::SERIAL_38400;
            break;
        case 76800:
            serialSpeed = port::SERIAL_76800;
            break;
        case 230400:
            serialSpeed = port::SERIAL_230400;
            break;
        case 460800:
            serialSpeed = port::SERIAL_460800;
            break;
        default:
            LogWarning(LOG_HOST, "Unsupported serial speed %u, defaulting to %u", uartSpeed, port::SERIAL_115200);
            uartSpeed = 115200;
        case 115200:
            break;
        }

        modemPort = new port::UARTPort(uartPort, serialSpeed, true);
        LogInfo("Modem Parameters");
        LogInfo("    UART Port: %s", uartPort.c_str());
        LogInfo("    UART Speed: %u", uartSpeed);
    }
    else if (portType == UDP_PORT) {
        ::LogError(LOG_HOST, "Calibration mode is unsupported with a remote modem!");
        return 2;
    }

    if (modemPort == NULL) {
        ::LogError(LOG_HOST, "Invalid modem port type, %s!", portType.c_str());
        return 2;
    }

    m_modem = new Modem(modemPort, false, false, false, false, true, false, 80, 7, 4, 10, false, false, false);

    m_modem->setOpenHandler(MODEM_OC_PORT_HANDLER_BIND(HostCal::portModemOpen, this));
    m_modem->setCloseHandler(MODEM_OC_PORT_HANDLER_BIND(HostCal::portModemClose, this));
    m_modem->setResponseHandler(MODEM_RESP_HANDLER_BIND(HostCal::portModemHandler, this));

    // open modem and initialize
    ret = m_modem->open();
    if (!ret) {
        ::LogError(LOG_CAL, "Failed to open modem");
        return 1;
    }

    // open terminal console
    ret = m_console.open();
    if (!ret) {
        return 1;
    }

    displayHelp();

    m_rxInvert = modemConf["rxInvert"].as<bool>(false);
    m_txInvert = modemConf["txInvert"].as<bool>(false);
    m_pttInvert = modemConf["pttInvert"].as<bool>(false);
    m_dcBlocker = modemConf["dcBlocker"].as<bool>(true);

    m_rxDCOffset = modemConf["rxDCOffset"].as<int>(0);
    m_txDCOffset = modemConf["txDCOffset"].as<int>(0);
    m_rxLevel = modemConf["rxLevel"].as<float>(50.0F);
    m_txLevel = modemConf["txLevel"].as<float>(50.0F);

    m_dmrSymLevel3Adj = modemConf["dmrSymLvl3Adj"].as<int>(0);
    m_dmrSymLevel1Adj = modemConf["dmrSymLvl1Adj"].as<int>(0);
    m_p25SymLevel3Adj = modemConf["p25SymLvl3Adj"].as<int>(0);
    m_p25SymLevel1Adj = modemConf["p25SymLvl1Adj"].as<int>(0);

    m_fdmaPreamble = (uint8_t)modemConf["fdmaPreamble"].as<uint32_t>(80U);
    m_dmrRxDelay = (uint8_t)modemConf["dmrRxDelay"].as<uint32_t>(7U);
    m_p25CorrCount = (uint8_t)modemConf["p25CorrCount"].as<uint32_t>(5U);

    writeConfig();

    printStatus();

    bool end = false;
    while (!end) {
        int c = m_console.getChar();
        switch (c) {
            /** Level Adjustment Commands */
        case 'I':
        {
            m_txInvert = !m_txInvert;
            LogMessage(LOG_CAL, " - TX Invert: %s", m_txInvert ? "On" : "Off");
            writeConfig();
        }
        break;
        case 'i':
        {
            m_rxInvert = !m_rxInvert;
            LogMessage(LOG_CAL, " - RX Invert: %s", m_rxInvert ? "On" : "Off");
            writeConfig();
        }
        break;
        case 'p':
        {
            m_pttInvert = !m_pttInvert;
            LogMessage(LOG_CAL, " - PTT Invert: %s", m_pttInvert ? "On" : "Off");
            writeConfig();
        }
        break;
        case 'd':
        {
            m_dcBlocker = !m_dcBlocker;
            LogMessage(LOG_CAL, " - DC Blocker: %s", m_dcBlocker ? "On" : "Off");
            writeConfig();
        }
        break;
        case 'D':
        {
            m_debug = !m_debug;
            LogMessage(LOG_CAL, " - Modem Debug: %s", m_debug ? "On" : "Off");
            writeConfig();
        }
        break;
        case 'R':
            setRXLevel(1);
            break;
        case 'r':
            setRXLevel(-1);
            break;
        case 'T':
            setTXLevel(1);
            break;
        case 't':
            setTXLevel(-1);
            break;
        case 'c':
            setRXDCOffset(-1);
            break;
        case 'C':
            setRXDCOffset(1);
            break;
        case 'o':
            setTXDCOffset(-1);
            break;
        case 'O':
            setTXDCOffset(1);
            break;

        case '-':
            setDMRSymLevel3Adj(-1);
            break;
        case '=':
            setDMRSymLevel3Adj(1);
            break;
        case '_':
            setDMRSymLevel1Adj(-1);
            break;
        case '+':
            setDMRSymLevel1Adj(1);
            break;

        case '[':
            setP25SymLevel3Adj(-1);
            break;
        case ']':
            setP25SymLevel3Adj(1);
            break;
        case '{':
            setP25SymLevel1Adj(-1);
            break;
        case '}':
            setP25SymLevel1Adj(1);
            break;

            /** Mode Commands */
        case 'Z':
        {
            m_mode = STATE_DMR_CAL;
            m_modeStr = DMR_CAL_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_dmrRx1K = false;
            m_p25Enabled = false;
            m_p25Rx1K = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'z':
        {
            m_mode = STATE_P25_CAL;
            m_modeStr = P25_CAL_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_dmrRx1K = false;
            m_p25Enabled = false;
            m_p25Rx1K = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'L':
        {
            m_mode = STATE_DMR_LF_CAL;
            m_modeStr = DMR_LF_CAL_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_dmrRx1K = false;
            m_p25Enabled = false;
            m_p25Rx1K = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'l':
        {
            m_mode = STATE_P25_LF_CAL;
            m_modeStr = P25_LF_CAL_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_dmrRx1K = false;
            m_p25Enabled = false;
            m_p25Rx1K = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'M':
        {
            m_mode = STATE_DMR_CAL_1K;
            m_modeStr = DMR_CAL_1K_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_dmrRx1K = false;
            m_p25Enabled = false;
            m_p25Rx1K = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'm':
        {
            m_mode = STATE_DMR_DMO_CAL_1K;
            m_modeStr = DMR_DMO_CAL_1K_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_dmrRx1K = false;
            m_p25Enabled = false;
            m_p25Rx1K = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'P':
        {
            m_mode = STATE_P25_CAL_1K;
            m_modeStr = P25_CAL_1K_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_dmrRx1K = false;
            m_p25Enabled = false;
            m_p25Rx1K = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'B':
        case 'J':
        {
            m_mode = STATE_DMR;
            if (c == 'J') {
                m_modeStr = DMR_FEC_1K_STR;
                m_dmrRx1K = true;
            }
            else {
                m_modeStr = DMR_FEC_STR;
                m_dmrRx1K = false;
            }
            m_duplex = false;
            m_dmrEnabled = true;
            m_p25Enabled = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'b':
        case 'j':
        {
            m_mode = STATE_P25;
            if (c == 'j') {
                m_modeStr = P25_FEC_1K_STR;
                m_p25Rx1K = true;
            }
            else {
                m_modeStr = P25_FEC_STR;
                m_p25Rx1K = false;
            }
            m_duplex = false;
            m_dmrEnabled = false;
            m_p25Enabled = true;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'x':
        {
            m_mode = STATE_RSSI_CAL;
            m_modeStr = RSSI_CAL_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_dmrRx1K = false;
            m_p25Enabled = false;
            m_p25Rx1K = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;

        /** General Commands */
        case ' ':
            setTransmit();
            break;
        case '`':
            printStatus();
            break;
        case 'V':
            getHostVersion();
            break;
        case 'v':
            m_modem->getFirmwareVersion();
            break;
        case 'H':
        case 'h':
            displayHelp();
            break;
        case 'S':
        case 's':
        {
            yaml::Serialize(m_conf, m_confFile.c_str(), yaml::SerializeConfig(4, 64, false, false));
            LogMessage(LOG_CAL, " - Saved configuration to %s", m_confFile.c_str());
        }
        break;
        case 'Q':
        case 'q':
            end = true;
            break;

        case 13:
        case 10:
        case -1:
            break;
        default:
            LogError(LOG_CAL, "Unknown command - %c (H/h for help)", c);
            break;
        }

        m_modem->clock(0U);

        timerClock();
        sleep(5U);
    }

    if (m_transmit)
        setTransmit();

    m_modem->close();
    m_console.close();
    return 0;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="modem"></param>
bool HostCal::portModemOpen(Modem* modem)
{
    sleep(2000U);

    bool ret = writeConfig();
    if (!ret) {
        ret = writeConfig();
        if (!ret) {
            LogError(LOG_MODEM, "Modem unresponsive to configuration set after 2 attempts. Stopping.");
            m_modem->close();
            return false;
        }
    }

    LogMessage(LOG_MODEM, "Modem Ready [Calibration Mode]");

    // handled modem open
    return true;
}

/// <summary>
///
/// </summary>
/// <param name="modem"></param>
bool HostCal::portModemClose(Modem* modem)
{
    // handled modem close
    return true;
}

/// <summary>
///
/// </summary>
/// <param name="modem"></param>
/// <param name="ms"></param>
/// <param name="rspType"></param>
/// <param name="rspDblLen"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool HostCal::portModemHandler(Modem* modem, uint32_t ms, RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len)
{
   if (rspType == RTM_OK && len > 0) {
        switch (buffer[2U]) {
        case CMD_CAL_DATA:
        {
            bool inverted = (buffer[3U] == 0x80U);
            short high = buffer[4U] << 8 | buffer[5U];
            short low = buffer[6U] << 8 | buffer[7U];
            short diff = high - low;
            short centre = (high + low) / 2;
            LogMessage(LOG_CAL, "Levels: inverted: %s, max: %d, min: %d, diff: %d, centre: %d", inverted ? "yes" : "no", high, low, diff, centre);
        }
        break;
        case CMD_RSSI_DATA:
        {
            uint16_t max = buffer[3U] << 8 | buffer[4U];
            uint16_t min = buffer[5U] << 8 | buffer[6U];
            uint16_t ave = buffer[7U] << 8 | buffer[8U];
            LogMessage(LOG_CAL, "RSSI: max: %u, min: %u, ave: %u", max, min, ave);
        }
        break;

        case CMD_DMR_DATA1:
        case CMD_DMR_DATA2:
            processDMRBER(buffer + 4U, buffer[3]);
            break;

        case CMD_DMR_LOST1:
        case CMD_DMR_LOST2:
        {
            LogMessage(LOG_CAL, "DMR Transmission lost, total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));

            if (m_dmrEnabled) {
                m_berBits = 0U;
                m_berErrs = 0U;
                m_berFrames = 0U;
                m_berUndecodableLC = 0U;
                m_berUncorrectable = 0U;
            }
        }
        break;

        case CMD_P25_DATA:
            processP25BER(buffer + 3U);
            break;

        case CMD_P25_LOST:
        {
            LogMessage(LOG_CAL, "P25 Transmission lost, total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));

            if (m_p25Enabled) {
                m_berBits = 0U;
                m_berErrs = 0U;
                m_berFrames = 0U;
                m_berUndecodableLC = 0U;
                m_berUncorrectable = 0U;
            }
        }
        break;

        case CMD_GET_STATUS:
        {
            uint8_t modemState = buffer[4U];
            bool tx = (buffer[5U] & 0x01U) == 0x01U;

            bool adcOverflow = (buffer[5U] & 0x02U) == 0x02U;
            bool rxOverflow = (buffer[5U] & 0x04U) == 0x04U;
            bool txOverflow = (buffer[5U] & 0x08U) == 0x08U;
            bool dacOverflow = (buffer[5U] & 0x20U) == 0x20U;

            LogMessage(LOG_CAL, " - Diagnostic Values [Modem State: %u, Transmitting: %d, ADC Overflow: %d, Rx Overflow: %d, Tx Overflow: %d, DAC Overflow: %d]",
                modemState, tx, adcOverflow, rxOverflow, txOverflow, dacOverflow);
        }
        break;

        case CMD_GET_VERSION:
        case CMD_ACK:
            break;

        case CMD_NAK:
            LogWarning(LOG_MODEM, "NAK, command = 0x%02X, reason = %u", buffer[3U], buffer[4U]);
            break;

        case CMD_DEBUG1:
        case CMD_DEBUG2:
        case CMD_DEBUG3:
        case CMD_DEBUG4:
        case CMD_DEBUG5:
        case CMD_DEBUG_DUMP:
            m_modem->printDebug(buffer, len);
            break;

        default:
            LogWarning(LOG_MODEM, "Unknown message, type = %02X", buffer[2U]);
            Utils::dump("Buffer dump", buffer, len);
            break;
        }
    }

    // handled modem response
    return true;
}

/// <summary>
/// Helper to print the calibration help to the console.
/// </summary>
void HostCal::displayHelp()
{
    LogMessage(LOG_CAL, "General Commands:");
    LogMessage(LOG_CAL, "    <space>  Toggle transmit");
    LogMessage(LOG_CAL, "    `        Display current settings and operation mode");
    LogMessage(LOG_CAL, "    V        Display version of host");
    LogMessage(LOG_CAL, "    v        Display version of firmware");
    LogMessage(LOG_CAL, "    H/h      Display help");
    LogMessage(LOG_CAL, "    S/s      Save calibration settings to configuration file");
    LogMessage(LOG_CAL, "    Q/q      Quit");
    LogMessage(LOG_CAL, "Level Adjustment Commands:");
    LogMessage(LOG_CAL, "    I        Toggle transmit inversion");
    LogMessage(LOG_CAL, "    i        Toggle receive inversion");
    LogMessage(LOG_CAL, "    p        Toggle PTT inversion");
    LogMessage(LOG_CAL, "    d        Toggle DC blocker");
    LogMessage(LOG_CAL, "    R/r      Increase/Decrease receive level");
    LogMessage(LOG_CAL, "    T/t      Increase/Decrease transmit level");
    LogMessage(LOG_CAL, "    C/c      Increase/Decrease RX DC offset level");
    LogMessage(LOG_CAL, "    O/o      Increase/Decrease TX DC offset level");
    LogMessage(LOG_CAL, "Mode Commands:");
    LogMessage(LOG_CAL, "    Z        %s", DMR_CAL_STR);
    LogMessage(LOG_CAL, "    z        %s", P25_CAL_STR);
    LogMessage(LOG_CAL, "    L        %s", DMR_LF_CAL_STR);
    LogMessage(LOG_CAL, "    l        %s", P25_LF_CAL_STR);
    LogMessage(LOG_CAL, "    M        %s", DMR_CAL_1K_STR);
    LogMessage(LOG_CAL, "    m        %s", DMR_DMO_CAL_1K_STR);
    LogMessage(LOG_CAL, "    P        %s", P25_CAL_1K_STR);
    LogMessage(LOG_CAL, "    B        %s", DMR_FEC_STR);
    LogMessage(LOG_CAL, "    J        %s", DMR_FEC_1K_STR);
    LogMessage(LOG_CAL, "    b        %s", P25_FEC_STR);
    LogMessage(LOG_CAL, "    j        %s", P25_FEC_1K_STR);
    LogMessage(LOG_CAL, "    x        %s", RSSI_CAL_STR);
    LogMessage(LOG_CAL, "Engineering Commands:");
    LogMessage(LOG_CAL, "    -/=      Increase/Decrease DMR +/- 3 Symbol Level");
    LogMessage(LOG_CAL, "    _/+      Increase/Decrease DMR +/- 1 Symbol Level");
    LogMessage(LOG_CAL, "    [/]      Increase/Decrease P25 +/- 3 Symbol Level");
    LogMessage(LOG_CAL, "    {/}      Increase/Decrease P25 +/- 1 Symbol Level");
}

/// <summary>
/// Helper to change the Rx level.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setTXLevel(int incr)
{
    if (incr > 0 && m_txLevel < 100.0F) {
        m_txLevel += 0.25F;

        // clamp values
        if (m_txLevel > 100.0F)
            m_txLevel = 100.0F;

        LogMessage(LOG_CAL, " - TX Level: %.1f%%", m_txLevel);
        return writeConfig();
    }

    if (incr < 0 && m_txLevel > 0.0F) {
        m_txLevel -= 0.25F;

        // clamp values
        if (m_txLevel < 0.0F)
            m_txLevel = 0.0F;

        LogMessage(LOG_CAL, " - TX Level: %.1f%%", m_txLevel);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the Rx level.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setRXLevel(int incr)
{
    if (incr > 0 && m_rxLevel < 100.0F) {
        m_rxLevel += 0.25F;

        // clamp values
        if (m_rxLevel > 100.0F)
            m_rxLevel = 100.0F;

        LogMessage(LOG_CAL, " - RX Level: %.1f%%", m_rxLevel);
        return writeConfig();
    }

    if (incr < 0 && m_rxLevel > 0.0F) {
        m_rxLevel -= 0.25F;

        // clamp values
        if (m_rxLevel < 0.0F)
            m_rxLevel = 0.0F;

        LogMessage(LOG_CAL, " - RX Level: %.1f%%", m_rxLevel);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the Tx DC offset.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setTXDCOffset(int incr)
{
    if (incr > 0 && m_txDCOffset < 127) {
        m_txDCOffset++;
        LogMessage(LOG_CAL, " - TX DC Offset: %d", m_txDCOffset);
        return writeConfig();
    }

    if (incr < 0 && m_txDCOffset > -127) {
        m_txDCOffset--;
        LogMessage(LOG_CAL, " - TX DC Offset: %d", m_txDCOffset);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the Rx DC offset.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setRXDCOffset(int incr)
{
    if (incr > 0 && m_rxDCOffset < 127) {
        m_rxDCOffset++;
        LogMessage(LOG_CAL, " - RX DC Offset: %d", m_rxDCOffset);
        return writeConfig();
    }

    if (incr < 0 && m_rxDCOffset > -127) {
        m_rxDCOffset--;
        LogMessage(LOG_CAL, " - RX DC Offset: %d", m_rxDCOffset);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the DMR Symbol Level 3 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setDMRSymLevel3Adj(int incr)
{
    if (incr > 0 && m_dmrSymLevel3Adj < 127) {
        m_dmrSymLevel3Adj++;
        LogMessage(LOG_CAL, " - DMR Symbol Level +/- 3 Adjust: %d", m_dmrSymLevel3Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_dmrSymLevel3Adj > -127) {
        m_dmrSymLevel3Adj--;
        LogMessage(LOG_CAL, " - DMR Symbol Level +/- 3 Adjust: %d", m_dmrSymLevel3Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to change the DMR Symbol Level 1 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setDMRSymLevel1Adj(int incr)
{
    if (incr > 0 && m_dmrSymLevel1Adj < 127) {
        m_dmrSymLevel1Adj++;
        LogMessage(LOG_CAL, " - DMR Symbol Level +/- 1 Adjust: %d", m_dmrSymLevel1Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_dmrSymLevel1Adj > -127) {
        m_dmrSymLevel1Adj--;
        LogMessage(LOG_CAL, " - DMR Symbol Level +/- 1 Adjust: %d", m_dmrSymLevel1Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to change the P25 Symbol Level 3 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setP25SymLevel3Adj(int incr)
{
    if (incr > 0 && m_p25SymLevel3Adj < 127) {
        m_p25SymLevel3Adj++;
        LogMessage(LOG_CAL, " - P25 Symbol Level +/- 3 Adjust: %d", m_p25SymLevel3Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_p25SymLevel3Adj > -127) {
        m_p25SymLevel3Adj--;
        LogMessage(LOG_CAL, " - P25 Symbol Level +/- 3 Adjust: %d", m_p25SymLevel3Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to change the P25 Symbol Level 1 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setP25SymLevel1Adj(int incr)
{
    if (incr > 0 && m_p25SymLevel1Adj < 127) {
        m_p25SymLevel1Adj++;
        LogMessage(LOG_CAL, " - P25 Symbol Level +/- 1 Adjust: %d", m_p25SymLevel1Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_p25SymLevel1Adj > -127) {
        m_p25SymLevel1Adj--;
        LogMessage(LOG_CAL, " - P25 Symbol Level +/- 1 Adjust: %d", m_p25SymLevel1Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to toggle modem transmit mode.
/// </summary>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setTransmit()
{
    if (m_dmrEnabled || m_p25Enabled) {
        LogError(LOG_CAL, "No transmit allowed in a BER Test mode");
        return false;
    }

    m_transmit = !m_transmit;

    uint8_t buffer[50U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_CAL_DATA;
    buffer[3U] = m_transmit ? 0x01U : 0x00U;

    int ret = m_modem->write(buffer, 4U);
    if (ret <= 0)
        return false;

    sleep(25U);

    if (m_transmit)
        LogMessage(LOG_CAL, " - Modem start transmitting");
    else
        LogMessage(LOG_CAL, " - Modem stop transmitting");

    m_modem->clock(0U);

    return true;
}

/// <summary>
/// Process DMR Rx BER.
/// </summary>
/// <param name="buffer">Buffer containing DMR audio frames</param>
/// <param name="seq">DMR Audio Sequence</param>
void HostCal::processDMRBER(const uint8_t* buffer, uint8_t seq)
{
    if (seq == 65U) {
        timerStart();

        LogMessage(LOG_CAL, "DMR voice header received");

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }
    else if (seq == 66U) {
        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, "DMR voice end received, total frames: %d, total bits: %d, uncorrectable frames: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
        }

        timerStop();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }

    timerStart();

    uint32_t errs = m_fec.measureDMRBER(buffer);

    float ber = float(errs) / 1.41F;
    if (ber < 10.0F)
        LogMessage(LOG_CAL, "DMR audio seq. %d, FEC BER %% (errs): %.3f%% (%u/141)", seq & 0x0FU, ber, errs);
    else {
        LogWarning(LOG_CAL, "uncorrectable DMR audio seq. %d", seq & 0x0FU);
        m_berUncorrectable++;
    }

    m_berBits += 141U;
    m_berErrs += errs;
    m_berFrames++;
}

/// <summary>
/// Process DMR Tx 1011hz BER.
/// </summary>
/// <param name="buffer">Buffer containing DMR audio frames</param>
/// <param name="seq">DMR Audio Sequence</param>
void HostCal::processDMR1KBER(const uint8_t* buffer, uint8_t seq)
{
    uint32_t errs = 0U;

    if (seq == 65U) {
        timerStart();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;

        for (uint32_t i = 0U; i < 33U; i++)
            errs += countErrs(buffer[i], VH_DMO1K[i]);

        m_berErrs += errs;
        m_berBits += 264;
        m_berFrames++;
        LogMessage(LOG_CAL, "DMR voice header received, 1031 Test Pattern BER %% (errs): %.3f%% (%u/264)", float(errs) / 2.64F, errs);
    }
    else if (seq == 66U) {
        for (uint32_t i = 0U; i < 33U; i++)
            errs += countErrs(buffer[i], VT_DMO1K[i]);

        m_berErrs += errs;
        m_berBits += 264;
        m_berFrames++;

        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, "DMR voice end received, total frames: %d, total bits: %d, uncorrectable frames: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
        }

        timerStop();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }

    timerStart();

    uint8_t dmrSeq = seq & 0x0FU;
    if (dmrSeq > 5U)
        dmrSeq = 5U;
    
    errs = 0U;
    for (uint32_t i = 0U; i < 33U; i++)
        errs += countErrs(buffer[i], VOICE_1K[i]);

    float ber = float(errs) / 2.64F;

    m_berErrs += errs;
    m_berBits += 264;
    m_berFrames++;

    if (ber < 10.0F)
        LogMessage(LOG_CAL, "DMR audio seq. %d, 1031 Test Pattern BER %% (errs): %.3f%% (%u/264)", seq & 0x0FU, ber, errs);
    else {
        LogWarning(LOG_CAL, "uncorrectable DMR audio seq. %d", seq & 0x0FU);
        m_berUncorrectable++;
    }
}

/// <summary>
/// Process P25 Rx BER.
/// </summary>
/// <param name="buffer">Buffer containing P25 audio frames</param>
void HostCal::processP25BER(const uint8_t* buffer)
{
    using namespace p25;

    uint8_t sync[P25_SYNC_LENGTH_BYTES];
    ::memcpy(sync, buffer + 1U, P25_SYNC_LENGTH_BYTES);

    uint8_t syncErrs = 0U;
    for (uint8_t i = 0U; i < P25_SYNC_LENGTH_BYTES; i++)
        syncErrs += Utils::countBits8(sync[i] ^ P25_SYNC_BYTES[i]);

    LogMessage(LOG_CAL, "P25, sync word, errs = %u, sync word = %02X %02X %02X %02X %02X %02X", syncErrs,
        sync[0U], sync[1U], sync[2U], sync[3U], sync[4U], sync[5U]);

    uint8_t nid[P25_NID_LENGTH_BYTES];
    P25Utils::decode(buffer + 1U, nid, 48U, 114U);
    uint8_t duid = nid[1U] & 0x0FU;

    uint32_t errs = 0U;
    uint8_t imbe[18U];
    lc::LC lc = lc::LC();
    data::DataHeader dataHeader = data::DataHeader();

    if (duid == P25_DUID_HDU) {
        timerStart();

        bool ret = lc.decodeHDU(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_HDU (Header), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_HDU (Header), dstId = %u, algo = %X, kid = %X", lc.getDstId(), lc.getAlgId(), lc.getKId());
        }
        
        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
    }
    else if (duid == P25_DUID_TDU) {
        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, "P25_DUID_TDU (Terminator Data Unit), total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
        }

        timerStop();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }
    else if (duid == P25_DUID_LDU1) {
        timerStart();

        bool ret = lc.decodeLDU1(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1) LC, mfId = $%02X, lco = $%02X, emerg = %u, encrypt = %u, prio = %u, group = %u, srcId = %u, dstId = %u", 
                lc.getMFId(), lc.getLCO(), lc.getEmergency(), lc.getEncrypted(), lc.getPriority(), lc.getGroup(), lc.getSrcId(), lc.getDstId());
        }

        P25Utils::decode(buffer + 1U, imbe, 114U, 262U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 262U, 410U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 452U, 600U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 640U, 788U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 830U, 978U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1020U, 1168U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1208U, 1356U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1398U, 1546U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1578U, 1726U);
        errs += m_fec.measureP25BER(imbe);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), audio FEC BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), uncorrectable audio");
            m_berUncorrectable++;
        }

        m_berBits += 1233U;
        m_berErrs += errs;
        m_berFrames++;
    }
    else if (duid == P25_DUID_LDU2) {
        timerStart();

        bool ret = lc.decodeLDU2(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2) LC, mfId = $%02X, algo = %X, kid = %X", 
                lc.getMFId(), lc.getAlgId(), lc.getKId());
        }

        P25Utils::decode(buffer + 1U, imbe, 114U, 262U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 262U, 410U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 452U, 600U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 640U, 788U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 830U, 978U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1020U, 1168U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1208U, 1356U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1398U, 1546U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1578U, 1726U);
        errs += m_fec.measureP25BER(imbe);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), audio FEC BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), uncorrectable audio");
            m_berUncorrectable++;
        }

        m_berBits += 1233U;
        m_berErrs += errs;
        m_berFrames++;
    }
    else if (duid == P25_DUID_PDU) {
        timerStop();

        // note: for the calibrator we will only process the PDU header -- and not the PDU data
        uint8_t pduBuffer[P25_LDU_FRAME_LENGTH_BYTES];
        uint32_t bits = P25Utils::decode(buffer + 1U, pduBuffer, 0, P25_LDU_FRAME_LENGTH_BITS);

        uint8_t* rfPDU = new uint8_t[P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U];
        ::memset(rfPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);
        uint32_t rfPDUBits = 0U;

        for (uint32_t i = 0U; i < bits; i++, rfPDUBits++) {
            bool b = READ_BIT(buffer, i);
            WRITE_BIT(rfPDU, rfPDUBits, b);
        }

        bool ret = dataHeader.decode(rfPDU + P25_SYNC_LENGTH_BYTES + P25_NID_LENGTH_BYTES);
        if (!ret) {
            LogWarning(LOG_RF, "P25_DUID_PDU (Packet Data Unit), unfixable RF 1/2 rate header data");
            Utils::dump(1U, "Unfixable PDU Data", rfPDU + P25_SYNC_LENGTH_BYTES + P25_NID_LENGTH_BYTES, P25_PDU_HEADER_LENGTH_BYTES);
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_PDU (Packet Data Unit), fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u",
                dataHeader.getFormat(), dataHeader.getSAP(), dataHeader.getFullMessage(), dataHeader.getBlocksToFollow(), dataHeader.getPadCount(),
                dataHeader.getN(), dataHeader.getSeqNo());
        }

        delete[] rfPDU;
    }
    else if (duid == P25_DUID_TSDU) {
        timerStop();

        lc::TSBK tsbk = lc::TSBK(SiteData(), lookups::IdenTable());
        bool ret = tsbk.decode(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_TSDU (Trunking System Data Unit), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_TSDU (Trunking System Data Unit), mfId = $%02X, lco = $%02X, srcId = %u, dstId = %u, service = %u, status = %u, message = %u, extFunc = %u, netId = %u, sysId = %u",
                tsbk.getMFId(), tsbk.getLCO(), tsbk.getSrcId(), tsbk.getDstId(), tsbk.getService(), tsbk.getStatus(), tsbk.getMessage(), tsbk.getExtendedFunction(),
                tsbk.getNetId(), tsbk.getSysId());
        }
    }
}

/// <summary>
/// Process P25 Tx 1011hz BER.
/// </summary>
/// <param name="buffer">Buffer containing P25 audio frames</param>
void HostCal::processP251KBER(const uint8_t* buffer)
{
    using namespace p25;

    uint8_t nid[P25_NID_LENGTH_BYTES];
    P25Utils::decode(buffer + 1U, nid, 48U, 114U);
    uint8_t duid = nid[1U] & 0x0FU;

    uint32_t errs = 0U;
    lc::LC lc = lc::LC();

    if (duid == P25_DUID_HDU) {
        timerStart();

        bool ret = lc.decodeHDU(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_HDU (Header), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_RF, "P25_DUID_HDU (Header), dstId = %u, algo = %X, kid = %X", lc.getDstId(), lc.getAlgId(), lc.getKId());
        }

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
    }
    else if (duid == P25_DUID_TDU) {
        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, "P25_DUID_TDU (Terminator Data Unit), total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
        }

        timerStop();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }
    else if (duid == P25_DUID_LDU1) {
        timerStart();

        bool ret = lc.decodeLDU1(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1) LC, mfId = $%02X, lco = $%02X, emerg = %u, encrypt = %u, prio = %u, group = %u, srcId = %u, dstId = %u",
                lc.getMFId(), lc.getLCO(), lc.getEmergency(), lc.getEncrypted(), lc.getPriority(), lc.getGroup(), lc.getSrcId(), lc.getDstId());
        }

        for (uint32_t i = 0U; i < 216U; i++)
            errs += countErrs(buffer[i + 1U], LDU1_1K[i]);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), 1011 Test Pattern BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), uncorrectable audio");
            m_berUncorrectable++;
        }

        m_berBits += 1233U;
        m_berErrs += errs;
        m_berFrames++;
    }
    else if (duid == P25_DUID_LDU2) {
        timerStart();

        bool ret = lc.decodeLDU2(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2) LC, mfId = $%02X, algo = %X, kid = %X",
                lc.getMFId(), lc.getAlgId(), lc.getKId());
        }

        for (uint32_t i = 0U; i < 216U; i++)
            errs += countErrs(buffer[i + 1U], LDU2_1K[i]);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), 1011 Test Pattern BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), uncorrectable audio");
            m_berUncorrectable++;
        }

        m_berBits += 1233U;
        m_berErrs += errs;
        m_berFrames++;
    }
}

/// <summary>
/// Write configuration to the modem DSP.
/// </summary>
/// <returns>True, if configuration is written, otherwise false.</returns>
bool HostCal::writeConfig()
{
    return writeConfig(m_mode);
}

/// <summary>
/// Write configuration to the modem DSP.
/// </summary>
/// <param name="modeOverride"></param>
/// <returns>True, if configuration is written, otherwise false.</returns>
bool HostCal::writeConfig(uint8_t modeOverride)
{
    uint8_t buffer[50U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 17U;
    buffer[2U] = CMD_SET_CONFIG;

    buffer[3U] = 0x00U;
    m_conf["system"]["modem"]["rxInvert"] = __BOOL_STR(m_rxInvert);
    if (m_rxInvert)
        buffer[3U] |= 0x01U;
    m_conf["system"]["modem"]["txInvert"] = __BOOL_STR(m_txInvert);
    if (m_txInvert)
        buffer[3U] |= 0x02U;
    m_conf["system"]["modem"]["pttInvert"] = __BOOL_STR(m_pttInvert);
    if (m_pttInvert)
        buffer[3U] |= 0x04U;
    if (m_debug)
        buffer[3U] |= 0x10U;
    if (!m_duplex)
        buffer[3U] |= 0x80U;

    buffer[4U] = 0x00U;
    m_conf["system"]["modem"]["dcBlocker"] = __BOOL_STR(m_dcBlocker);
    if (m_dcBlocker)
        buffer[4U] |= 0x01U;

    if (m_dmrEnabled)
        buffer[4U] |= 0x02U;
    if (m_p25Enabled)
        buffer[4U] |= 0x08U;

    if (m_fdmaPreamble > MAX_FDMA_PREAMBLE) {
        LogWarning(LOG_P25, "oversized FDMA preamble count, reducing to maximum %u", MAX_FDMA_PREAMBLE);
        m_fdmaPreamble = MAX_FDMA_PREAMBLE;
    }

    buffer[5U] = m_fdmaPreamble;

    buffer[6U] = modeOverride;

    m_conf["system"]["modem"]["rxLevel"] = __FLOAT_STR(m_rxLevel);
    buffer[7U] = (uint8_t)(m_rxLevel * 2.55F + 0.5F);

    m_conf["system"]["modem"]["txLevel"] = __FLOAT_STR(m_txLevel);
    buffer[8U] = (uint8_t)(m_txLevel * 2.55F + 0.5F);

    buffer[9U] = 1U;

    buffer[10U] = m_dmrRxDelay;
    
    uint32_t nac = 0xF7EU;
    buffer[11U] = (nac >> 4) & 0xFFU;
    buffer[12U] = (nac << 4) & 0xF0U;

    buffer[13U] = (uint8_t)(m_txLevel * 2.55F + 0.5F);
    buffer[15U] = (uint8_t)(m_txLevel * 2.55F + 0.5F);

    m_conf["system"]["modem"]["txDCOffset"] = __INT_STR(m_txDCOffset);
    buffer[16U] = (uint8_t)(m_txDCOffset + 128);
    m_conf["system"]["modem"]["rxDCOffset"] = __INT_STR(m_rxDCOffset);
    buffer[17U] = (uint8_t)(m_rxDCOffset + 128);

    buffer[14U] = (uint8_t)m_p25CorrCount;

    int ret = m_modem->write(buffer, 17U);
    if (ret <= 0)
        return false;

    sleep(10U);

    m_modem->clock(0U);
    return true;
}

/// <summary>
/// Write symbol level adjustments to the modem DSP.
/// </summary>
/// <returns>True, if level adjustments are written, otherwise false.</returns>
bool HostCal::writeSymbolAdjust()
{
    uint8_t buffer[10U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 7U;
    buffer[2U] = CMD_SET_SYMLVLADJ;

    m_conf["system"]["modem"]["dmrSymLvl3Adj"] = __INT_STR(m_dmrSymLevel3Adj);
    buffer[3U] = (uint8_t)(m_dmrSymLevel3Adj + 128);
    m_conf["system"]["modem"]["dmrSymLvl1Adj"] = __INT_STR(m_dmrSymLevel1Adj);
    buffer[4U] = (uint8_t)(m_dmrSymLevel1Adj + 128);

    m_conf["system"]["modem"]["p25SymLvl3Adj"] = __INT_STR(m_p25SymLevel3Adj);
    buffer[5U] = (uint8_t)(m_p25SymLevel3Adj + 128);
    m_conf["system"]["modem"]["p25SymLvl1Adj"] = __INT_STR(m_p25SymLevel1Adj);
    buffer[6U] = (uint8_t)(m_p25SymLevel1Adj + 128);

    int ret = m_modem->write(buffer, 7U);
    if (ret <= 0)
        return false;

    sleep(10U);

    m_modem->clock(0U);
    return true;
}

/// <summary>
/// Helper to sleep the calibration thread.
/// </summary>
/// <param name="ms">Milliseconds to sleep.</param>
void HostCal::sleep(uint32_t ms)
{
#if defined(_WIN32) || defined(_WIN64)
    ::Sleep(ms);
#else
    ::usleep(ms * 1000);
#endif
}

/// <summary>
/// Helper to clock the calibration BER timer.
/// </summary>
void HostCal::timerClock()
{
    if (m_timer > 0U && m_timeout > 0U) {
        m_timer += 1U;

        if (m_timer >= m_timeout) {
            LogMessage(LOG_CAL, "Transmission lost, total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));

            m_berBits = 0U;
            m_berErrs = 0U;
            m_berFrames = 0U;
            m_berUndecodableLC = 0U;
            m_berUncorrectable = 0U;

            timerStop();
        }
    }
}

/// <summary>
/// Helper to start the calibration BER timer.
/// </summary>
void HostCal::timerStart()
{
    if (m_timeout > 0U)
        m_timer = 1U;
}

/// <summary>
/// Helper to stop the calibration BER timer.
/// </summary>
void HostCal::timerStop()
{
    m_timer = 0U;
}

/// <summary>
/// Prints the current status of the calibration.
/// </summary>
void HostCal::printStatus()
{
    LogMessage(LOG_CAL, " - PTT Invert: %s, RX Invert: %s, TX Invert: %s, DC Blocker: %s",
        m_pttInvert ? "yes" : "no", m_rxInvert ? "yes" : "no", m_txInvert ? "yes" : "no", m_dcBlocker ? "yes" : "no");
    LogMessage(LOG_CAL, " - RX Level: %.1f%%, TX Level: %.1f%%, TX DC Offset: %d, RX DC Offset: %d",
        m_rxLevel, m_txLevel, m_txDCOffset, m_rxDCOffset);
    LogMessage(LOG_CAL, " - DMR Symbol +/- 3 Level Adj.: %d, DMR Symbol +/- 1 Level Adj.: %d, P25 Symbol +/- 3 Level Adj.: %d, P25 Symbol +/- 1 Level Adj.: %d",
        m_dmrSymLevel3Adj, m_dmrSymLevel1Adj, m_p25SymLevel3Adj, m_p25SymLevel1Adj);
    LogMessage(LOG_CAL, " - FDMA Preambles: %u (%.1fms), DMR Rx Delay: %u (%.1fms), P25 Corr. Count: %u (%.1fms)", m_fdmaPreamble, float(m_fdmaPreamble) * 0.2083F, m_dmrRxDelay, float(m_dmrRxDelay) * 0.0416666F,
        m_p25CorrCount, float(m_p25CorrCount) * 0.667F);
    LogMessage(LOG_CAL, " - Operating Mode: %s", m_modeStr.c_str());

    uint8_t buffer[50U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_GET_STATUS;

    int ret = m_modem->write(buffer, 4U);
    if (ret <= 0)
        return;

    sleep(25U);

    m_modem->clock(0U);
}

/// <summary>
/// Counts the total number of bit errors between bytes.
/// </summary>
/// <param name="a"></param>
/// <param name="b"></param>
/// <returns></returns>
uint8_t HostCal::countErrs(uint8_t a, uint8_t b)
{
    int cnt = 0;
    uint8_t tmp = a ^ b;
    while (tmp) {
        if (tmp % 2 == 1)
            cnt++;
        tmp /= 2;
    }
    return cnt;
}
