/********************************************************************************
* FaceTrackNoIR		This program is a private project of the some enthusiastic	*
*					gamers from Holland, who don't like to pay much for			*
*					head-tracking.												*
*																				*
* Copyright (C) 2013	Wim Vriend (Developing)									*
*						Ron Hendriks (Researching and Testing)					*
*																				*
* Homepage																		*
*																				*
* This program is free software; you can redistribute it and/or modify it		*
* under the terms of the GNU General Public License as published by the			*
* Free Software Foundation; either version 3 of the License, or (at your		*
* option) any later version.													*
*																				*
* This program is distributed in the hope that it will be useful, but			*
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY	*
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for	*
* more details.																	*
*																				*
* You should have received a copy of the GNU General Public License along		*
* with this program; if not, see <http://www.gnu.org/licenses/>.				*
*																				*
* FTServer		FTServer is the Class, that communicates headpose-data			*
*				to games, using the FreeTrackClient.dll.	         			*
********************************************************************************/
#include <algorithm>
#include "ftnoir_protocol_ft.h"
#include "ftnoir_csv/csv.h"

/** constructor **/
FTNoIR_Protocol::FTNoIR_Protocol() :
    shm(FT_MM_DATA, FREETRACK_MUTEX, sizeof(FTMemMap))
{
    pMemData = (FTMemMap*) shm.mem;
	useTIRViews	= false;
	useDummyExe	= false;
    intUsedInterface = 0;
	
    loadSettings();

	ProgramName = "";
	intGameID = 0;

	viewsStart = 0;
	viewsStop = 0;
}

/** destructor **/
FTNoIR_Protocol::~FTNoIR_Protocol()
{

	qDebug()<< "~FTNoIR_Protocol: Destructor started.";

	//
	// Stop if started
	//
	if (viewsStop != NULL) {
		qDebug()<< "~FTNoIR_Protocol: Stopping TIRViews.";
		viewsStop();
		FTIRViewsLib.unload();
	}
}

//
// Read the game-data from CSV
//


//
// Load the current Settings from the currently 'active' INI-file.
//
void FTNoIR_Protocol::loadSettings() {
	QSettings settings("opentrack");	// Registry settings (in HK_USER)

	QString currentFile = settings.value ( "SettingsFile", QCoreApplication::applicationDirPath() + "/settings/default.ini" ).toString();
	QSettings iniFile( currentFile, QSettings::IniFormat );		// Application settings (in INI-file)

	iniFile.beginGroup ( "FT" );
	intUsedInterface = iniFile.value ( "UsedInterface", 0 ).toInt();
	iniFile.endGroup ();

	//
	// Use the settings-section from the deprecated fake-TIR protocol, as they are most likely to be found there.
	//
	iniFile.beginGroup ( "FTIR" );
	useTIRViews	= iniFile.value ( "useTIRViews", 0 ).toBool();
	useDummyExe	= iniFile.value ( "useDummyExe", 0 ).toBool();
	iniFile.endGroup ();
}

//
// Update Headpose in Game.
//
void FTNoIR_Protocol::sendHeadposeToGame(double *headpose, double *rawheadpose ) {
float virtPosX;
float virtPosY;
float virtPosZ;

float virtRotX;
float virtRotY;
float virtRotZ;

float headPosX;
float headPosY;
float headPosZ;

float headRotX;
float headRotY;
float headRotZ;

    //
	// Scale the Raw measurements to the client measurements.
	//
    headRotX = getRadsFromDegrees(rawheadpose[Pitch]);
    headRotY = getRadsFromDegrees(rawheadpose[Yaw]);
    headRotZ = getRadsFromDegrees(rawheadpose[Roll]);
    headPosX = rawheadpose[TX] * 10;
    headPosY = rawheadpose[TY] * 10;
    headPosZ = rawheadpose[TZ] * 10;

    virtRotX = getRadsFromDegrees(headpose[Pitch]);
    virtRotY = getRadsFromDegrees(headpose[Yaw]);
    virtRotZ = getRadsFromDegrees(headpose[Roll]);
    virtPosX = headpose[TX] * 10;
    virtPosY = headpose[TY] * 10;
    virtPosZ = headpose[TZ] * 10;

    shm.lock();
    
    pMemData->data.RawX = headPosX;
    pMemData->data.RawY = headPosY;
    pMemData->data.RawZ = headPosZ;
    pMemData->data.RawPitch = headRotX;
    pMemData->data.RawYaw = headRotY;
    pMemData->data.RawRoll = headRotZ;

    //
    //
    pMemData->data.X = virtPosX;
    pMemData->data.Y = virtPosY;
    pMemData->data.Z = virtPosZ;
    pMemData->data.Pitch = virtRotX;
    pMemData->data.Yaw = virtRotY;
    pMemData->data.Roll = virtRotZ;

    //
    // Leave some values 0 yet...
    //
    pMemData->data.X1 = pMemData->data.DataID + 10;
    pMemData->data.X2 = 0;
    pMemData->data.X3 = 0;
    pMemData->data.X4 = 0;
    pMemData->data.Y1 = 0;
    pMemData->data.Y2 = 0;
    pMemData->data.Y3 = 0;
    pMemData->data.Y4 = 0;

    //
    // Check if the handle that was sent to the Game, was changed (on x64, this will be done by the ED-API)
    // If the "Report Program Name" command arrives (which is a '1', for now), raise the event from here!
    //
    //
    // The game-ID was changed?
    //
    if (intGameID != pMemData->GameID)
    {
        QString gamename;
        CSV::getGameData(pMemData->GameID, pMemData->table, gamename);
        pMemData->GameID2 = pMemData->GameID;
        intGameID = pMemData->GameID;
        QMutexLocker foo(&this->game_name_mutex);
        connected_game = gamename;
    }

	pMemData->data.DataID += 1;
    
    shm.unlock();
}

void FTNoIR_Protocol::start_tirviews() {
    QString aFileName = QCoreApplication::applicationDirPath() + "/TIRViews.dll";
    if ( QFile::exists( aFileName )) {
        FTIRViewsLib.setFileName(aFileName);
        FTIRViewsLib.load();

        viewsStart = (importTIRViewsStart) FTIRViewsLib.resolve("TIRViewsStart");
        if (viewsStart == NULL) {
            qDebug() << "FTServer::run() says: TIRViewsStart function not found in DLL!";
        }
        else {
            qDebug() << "FTServer::run() says: TIRViewsStart executed!";
            viewsStart();
        }

        //
        // Load the Stop function from TIRViews.dll. Call it when terminating the thread.
        //
        viewsStop = (importTIRViewsStop) FTIRViewsLib.resolve("TIRViewsStop");
        if (viewsStop == NULL) {
            qDebug() << "FTServer::run() says: TIRViewsStop function not found in DLL!";
        }
    }
}

void FTNoIR_Protocol::start_dummy() {
    QString program = QCoreApplication::applicationDirPath() + "/TrackIR.exe";
    dummyTrackIR.startDetached("\"" + program + "\"");

    qDebug() << "FTServer::run() says: TrackIR.exe executed!" << program;
}

bool FTNoIR_Protocol::checkServerInstallationOK()
{   
	QSettings settings("Freetrack", "FreetrackClient");							// Registry settings (in HK_USER)
	QSettings settingsTIR("NaturalPoint", "NATURALPOINT\\NPClient Location");	// Registry settings (in HK_USER)
	QString aLocation;															// Location of Client DLL

	qDebug() << "checkServerInstallationOK says: Starting Function";

    //
    // Write the path in the registry (for FreeTrack and FreeTrack20), for the game(s).
    //
    aLocation =  QCoreApplication::applicationDirPath() + "/";

    qDebug() << "checkServerInstallationOK says: used interface = " << intUsedInterface;
    switch (intUsedInterface) {
        case 0:									// Use both interfaces
            settings.setValue( "Path" , aLocation );
            settingsTIR.setValue( "Path" , aLocation );
            break;
        case 1:									// Use FreeTrack, disable TrackIR
            settings.setValue( "Path" , aLocation );
            settingsTIR.setValue( "Path" , "" );
            break;
        case 2:									// Use TrackIR, disable FreeTrack
            settings.setValue( "Path" , "" );
            settingsTIR.setValue( "Path" , aLocation );
            break;
        default:
            // should never be reached
        break;
    }

    //
    // TIRViews must be started first, or the NPClient DLL will never be loaded.
    //
    if (useTIRViews) {
        start_tirviews();
    }

    //
    // Check if TIRViews or dummy TrackIR.exe is required for this game
    //
    if (useDummyExe) {
        start_dummy();
    }
    
    if (!shm.success())
        return false;
    
    pMemData->data.DataID = 1;
    pMemData->data.CamWidth = 100;
    pMemData->data.CamHeight = 250;
    pMemData->GameID2 = 0;
    memset(pMemData->table, 0, 8);
    
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Factory function that creates instances if the Protocol object.

// Export both decorated and undecorated names.
//   GetProtocol     - Undecorated name, which can be easily used with GetProcAddress
//                Win32 API function.
//   _GetProtocol@0  - Common name decoration for __stdcall functions in C language.
//#pragma comment(linker, "/export:GetProtocol=_GetProtocol@0")

extern "C" FTNOIR_PROTOCOL_BASE_EXPORT IProtocol* CALLING_CONVENTION GetConstructor()
{
    return new FTNoIR_Protocol;
}
