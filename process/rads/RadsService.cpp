/*
* This file is part of BRAT 
*
* BRAT is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* BRAT is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "stdafx.h"

#include <QLocalServer>
#include <QLocalSocket>

#include "new-gui/Common/QtUtils.h"
#include "new-gui/Common/QtUtilsIO.h"
#include "new-gui/Common/System/OsProcess.h"

#include "simplecrypt.h"
#include "RadsService.h"
#include "RadsServiceLogger.h"
#include "RadsSharedParameters.h"


//#define WAIT_FOR_DEBUGGER

// Ensure that WAIT_FOR_DEBUGGER is always undefined in release builds
//
#if !defined (DEBUG) && !defined (_DEBUG)
#undef WAIT_FOR_DEBUGGER
#endif



//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//				Service Log Management
//
//		(must be called before any logging occurs;
//		should be called periodically after that)
//
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////



void CheckServiceLogging( const CApplicationStaticPaths &brat_paths )
{
	static const auto qgis_env_var = "QGIS_LOG_FILE";
	static const signed log_files_time_to_live = 10;
	static const signed log_files_size_to_backup = 10 * 1024 * 1024;

	const std::string &log_path = brat_paths.mRadsServiceLogFilePath;

	// Ensure file logging

	if ( qgetenv( qgis_env_var ).isEmpty() )
		qputenv( qgis_env_var, log_path.c_str() );


	// Log file (re)creation

	if ( IsFile( log_path ) )
	{
		QFileInfo info( log_path.c_str() );
		if ( info.size() > log_files_size_to_backup )
		{
			std::string old_path = CreateUniqueFileName( log_path );			//old_path is a new path, to save old contents
			if ( old_path.empty() || !DuplicateFile( log_path, old_path ) )		//save "old" file
				LOG_WARN( "Could not create new log file. Trying to use existing one." );
			else
				RemoveFile( log_path );
		}
	}

	// Old log files cleanup

	QDir sourceDir( GetDirectoryFromPath( log_path ).c_str() );
	QString filter = ( GetBaseFilenameFromPath( log_path ) + ".*" ).c_str();
	QStringList files = sourceDir.entryList( QStringList() << filter, QDir::Files );
	QDateTime log_files_end_date = QDateTime::currentDateTime().addDays( -log_files_time_to_live );
	const std::string log_ext = GetLastExtensionFromPath( log_path );
	for ( auto &file : files )
	{
		if ( log_ext == GetLastExtensionFromPath( q2a( file ) ) )
			continue;

		file = sourceDir.absolutePath() + "/" + file;
		QFileInfo info( file );
		if ( info.lastModified() <= log_files_end_date )
		{
			RemoveFile( file );
		}
	}
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////
// temporary code/////////////////////////////////////////////////////////////////////////////////////////

int processArgs(int argc, char **argv)
{
	if (argc > 2) {
		QString arg1(argv[1]);
		if (arg1 == QLatin1String("-i") ||
			arg1 == QLatin1String("-install")) {
			if (argc > 2) {
				QString account;
				QString password;
				QString path(argv[2]);
				if (argc > 3)
					account = argv[3];
				if (argc > 4)
					password = argv[4];
				printf("The service %s installed.\n",
					(QtServiceController::install(path, account, password) ? "was" : "was not"));
				return 0;
			}
		} else {
			QString serviceName(argv[1]);
			QtServiceController controller(serviceName);
			QString option(argv[2]);
			if (option == QLatin1String("-u") ||
				option == QLatin1String("-uninstall")) {
				printf("The service \"%s\" %s uninstalled.\n",
					controller.serviceName().toLatin1().constData(),
					(controller.uninstall() ? "was" : "was not"));
				return 0;
			} else if (option == QLatin1String("-s") ||
				option == QLatin1String("-start")) {
				QStringList args;
				for (int i = 3; i < argc; ++i)
					args.append(QString::fromLocal8Bit(argv[i]));
				printf("The service \"%s\" %s started.\n",
					controller.serviceName().toLatin1().constData(),
					(controller.start(args) ? "was" : "was not"));
				return 0;
			} else if (option == QLatin1String("-t") ||
				option == QLatin1String("-terminate")) {
				printf("The service \"%s\" %s stopped.\n",
					controller.serviceName().toLatin1().constData(),
					(controller.stop() ? "was" : "was not"));
				return 0;
			} else if (option == QLatin1String("-p") ||
				option == QLatin1String("-pause")) {
				printf("The service \"%s\" %s paused.\n",
					controller.serviceName().toLatin1().constData(),
					(controller.pause() ? "was" : "was not"));
				return 0;
			} else if (option == QLatin1String("-r") ||
				option == QLatin1String("-resume")) {
				printf("The service \"%s\" %s resumed.\n",
					controller.serviceName().toLatin1().constData(),
					(controller.resume() ? "was" : "was not"));
				return 0;
			} else if (option == QLatin1String("-c") ||
				option == QLatin1String("-command")) {
				if (argc > 3) {
					QString codestr(argv[3]);
					int code = codestr.toInt();
					printf("The command %s sent to the service \"%s\".\n",
						(controller.sendCommand(code) ? "was" : "was not"),
						controller.serviceName().toLatin1().constData());
					return 0;
				}
			} else if (option == QLatin1String("-v") ||
				option == QLatin1String("-version")) {
				bool installed = controller.isInstalled();
				printf("The service\n"
					"\t\"%s\"\n\n", controller.serviceName().toLatin1().constData());
				printf(" and %s\n\n", (controller.isRunning() ? "running" : "not running"));
				if (installed) {
					printf("path: %s\n", controller.serviceFilePath().toLatin1().data());
					printf("description: %s\n", controller.serviceDescription().toLatin1().data());
					printf("startup: %s\n", controller.startupType() == QtServiceController::AutoStartup ? "Auto" : "Manual");
				}
				return 0;
			}
		}
	}
	printf("controller [-i PATH | SERVICE_NAME [-v | -u | -s | -t | -p | -r | -c CODE] | -h] [-w]\n\n"
		"\t-i(nstall) PATH\t: Install the service\n"
		"\t-v(ersion)\t: Print status of the service\n"
		"\t-u(ninstall)\t: Uninstall the service\n"
		"\t-s(tart)\t: Start the service\n"
		"\t-t(erminate)\t: Stop the service\n"
		"\t-p(ause)\t: Pause the service\n"
		"\t-r(esume)\t: Resume the service\n"
		"\t-c(ommand) CODE\t: Send a command to the service\n"
		"\t-h(elp)\t\t: Print this help info\n"
		"\t-w(ait)\t\t: Wait for keypress when done\n");
	return 0;
}

int main_example(int argc, char **argv)
{
#if !defined(Q_OS_WIN)
	// QtService stores service settings in SystemScope, which normally require root privileges.
	// To allow testing this example as non-root, we change the directory of the SystemScope settings file.
	QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, QDir::tempPath());
	qWarning("(Example uses dummy settings file: %s/QtSoftware.conf)", QDir::tempPath().toLatin1().constData());
#endif

	int result = processArgs(argc, argv);

	if (QString::fromLocal8Bit(argv[argc-1]) == QLatin1String("-w") ||
		QString::fromLocal8Bit(argv[argc-1]) == QLatin1String("-wait")) {
		printf("\nPress Enter to continue...");
		QFile input;
		input.open(stdin, QIODevice::ReadOnly);
		input.readLine();
		printf("\n");
	}

	return result;
}

//////////////////////////////////////////////////////////
//						Password
//////////////////////////////////////////////////////////


static const quint64 default_key = 6396014356587936538;

inline QString encrypt( const QString &to_encrypt, quint64 key = default_key )
{
	SimpleCrypt processSimpleCrypt( key );
	return processSimpleCrypt.encryptToString( to_encrypt );
}
inline QString decrypt( const QString &to_decrypt, quint64 key = default_key )
{
	SimpleCrypt processSimpleCrypt( key );
	return processSimpleCrypt.decryptToString( to_decrypt );
}
/*
QString to_decrypt = encrypt( "rat@tu1" );
qDebug() << "Encrypted rads_password to" << to_decrypt;
qDebug() << "Decrypted rads_password to" << decrypt( to_decrypt );

const quint64 key = 89473829;
QString to_decrypt = encrypt( "rat@tu1", key );
qDebug() << "Encrypted rads_password to" << to_decrypt;
qDebug() << "Decrypted rads_password to" << decrypt( to_decrypt, key );
////Set The Encryption And Decryption Key
//SimpleCrypt processSimpleCrypt(89473829);
//QString rads_password("rat@tu1");
////Encrypt
//QString rads_password_encrypted = processSimpleCrypt.encryptToString( rads_password );
//qDebug() << "Encrypted rads_password to" << rads_password_encrypted;
////Decrypt
//QString decrypt = processSimpleCrypt.decryptToString( rads_password_encrypted );
//qDebug() << "Decrypted rads_password to" << decrypt;
*/



// temporary code/////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//						RadsClient
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////


CRadsClient::CRadsClient( CRadsSettings &settings, QObject *parent )	//parent = nullptr 
	: base_t( parent )
	, mSettings( settings ) 
	, mTimer( this )
	, mDisabled( false )
	, mRadsServerAddress( ReadRadsServerAddress( mSettings.RadsPaths().mRadsConfigurationFilePath ) )
	, mSharedMemory( RADS_SHARED_MEMORY_KEY.c_str(), this )
	, mSocketServer( new QLocalServer( this ) )
{
	//setup

	const int periodic_check_in_seconds = mSettings.PeriodicCheckInMinutes() * 60;

	if ( mSocketServer->listen( RADS_SHARED_MEMORY_KEY.c_str() ) )
		connect( mSocketServer, SIGNAL( newConnection() ), this, SLOT( HandleNewConnection() ) );
	else
		LOG_WARN( "Unable to start the RADS socket server: " + mSocketServer->errorString() );

	connect( &mTimer, &QTimer::timeout, this, &CRadsClient::HandleSynchronize );
	mTimer.start( periodic_check_in_seconds * 1000 );


	//startup (starting paused)

#if !defined (WAIT_FOR_DEBUGGER) && defined(PAUSE_ON_START)

	Pause();

#endif

	QTimer::singleShot( 0, this, &CRadsClient::HandleSynchronize );
}


//virtual 
CRadsClient::~CRadsClient()
{
	if ( mCurrentProcess )
	{
		CleanRsyncProcess( true );
		LOG_INFO( "Rads client is being deleted. A request was sent for existing rsync process to terminate." );
	}

	for ( auto *connection : mSocketConnections )
	{
		if ( !connection->isValid() )
			continue;

		connection->disconnectFromServer();
	}
}


//////////////////////////////////////////////////////////
//	RadsClient - what all this is about
//////////////////////////////////////////////////////////

void CRadsClient::Pause()
{
	mDisabled = true;
}


void CRadsClient::Resume()
{
	LOG_INFO( "RadsClient: resuming." );
	mDisabled = false;
	Synchronize();
}


void CRadsClient::DelaySaveConfig()
{
	if ( mSettings.HasFileLocked() )
	{
		QTimer::singleShot( 3000, this, &CRadsClient::DelaySaveConfig );
		LOG_INFO( "Configuration file is locked. Delaying save." );
	}
	else
	if ( mSettings.SaveConfig() )
		BroadcastRsyncStatusToSocketClients( CONFIG_UPDATED_SIGN.c_str() );
	else
		LOG_WARN( "RadsClient: Could not save the configuration to file." );
}


//for easy parameterless slot connect
void CRadsClient::ForceSynchronize()
{
	Synchronize( true );
}



template< typename STRING >
inline STRING QuotePath( const STRING &path )
{
	return "\"" + path + "\"";
}

//September 2016:
//server: corads.tudelft.nl
//user: radsuser
//pass. rat@tu1

//$RADS_ROOT/data/<sat>/<phase>/cycle/<sat>p<pass>c<cycle>.nc
//rsync -avrz --password-file=/cygdrive/l/bin/v140_xp/x64/Debug/brat/data/rads_pass.txt --del radsuser@corads.tudelft.nl::rads/tables .
//rsync -avrz --password-file=/cygdrive/l/bin/v140_xp/x64/Debug/brat/data/rads_pass.txt --del radsuser@corads.tudelft.nl::rads/data/j2 radsuser@corads.tudelft.nl::rads/data/c2 "/cygdrive/p/My Documents/brat/user-data/rads"
//rsync -avrzR --no-p --password-file=/cygdrive/l/bin/v140_xp/x64/Debug/brat/data/rads_pass.txt --del 
//radsuser@corads.tudelft.nl::rads/data/j2/a/c000 
//radsuser@corads.tudelft.nl::rads/data/c2/a/c004 "/cygdrive/p/My Documents/brat/user-data/rads"
//rd /q /s .\rads

//"/usr/bin/rsync" -avrzR --del --password-file="/Users/brat/dev/bin/Qt-5.7.0/brat/x86_64/Debug/brat.app/Contents/MacOS/data/rads_pass.txt" radsuser@corads.tudelft.nl::rads/data/c2 radsuser@corads.tudelft.nl::rads/data/j2  "/Volumes/ext_My_Passport/user-data/rads"
// /usr/bin/rsync -avrzR --del --password-file="/Users/brat/dev/bin/Qt-5.7.0/brat/x86_64/Debug/brat.app/Contents/MacOS/data/rads_pass.txt" radsuser@corads.tudelft.nl::'rads/data/c2 rads/data/j2'  "/Volumes/ext_My_Passport/user-data/rads"


bool CRadsClient::Synchronize( bool force )		//force = false 
{
	// ensure log is prepared

	CheckServiceLogging( mSettings.RadsPaths() );

	// ensure schedule

	bool need_save_config = mSettings.CorrectLastSyncTime();					assert__( !need_save_config );
	if ( need_save_config )
	{
		LOG_WARN( "Unexpected invalid synchronization time. It was corrected." );
	}

	if ( force )
	{
		LOG_INFO( "Trying to start immediate data synchronization with RADS." );
		need_save_config = mSettings.SetNextSyncDateToday() || need_save_config;
	}

	// ensure updated settings in file, if previous steps changed them

	if ( need_save_config )
		DelaySaveConfig();


	// check if it is not time to synchronize or it should otherwise be skipped

	if ( QDate::currentDate() < mSettings.NextSyncDate() )
	{
		return true;
	}

	bool disable = !force && mDisabled;
	if ( mCurrentProcess || disable || mSettings.OutputDirectory().empty() || mSettings.MissionNames().size() == 0 )
	{
		std::string msg = "Skipping one data synchronization event. Reason(s):\n";
		if ( mCurrentProcess )
			msg += "-An rsync process is still executing.\n";
		if ( disable )
			msg += "-The service was temporarily paused.\n";
		if ( mSettings.OutputDirectory().empty() )
			msg += "-No output directory was specified.\n";
		if ( mSettings.MissionNames().size() == 0 )             //see (*) below before changing
			msg += "-No missions were specified.\n";

		LOG_INFO( msg );
		return false;
	}


	// time to synchronize...


	//return BroadcastRsyncStatusToSocketClients();


    const std::string pass_file_path( mSettings.RadsPaths().mInternalDataDir + "/rads_pass.txt" );
    QFile pass_file( pass_file_path.c_str() );
    pass_file.setPermissions( QFile::ReadOwner | QFile::WriteOwner );

	std::string cmd_line = QuotePath( mSettings.RadsPaths().mRsyncExecutablePath );
	cmd_line += " ";
	cmd_line += "-avrzR --del --password-file=";
    cmd_line += QuotePath( win2cygwin( pass_file_path ) );		//TODO encrypt/decode: write unencrypted to temporary file passed here
	cmd_line += " ";
	std::string src_missions;
    
    //Assuming mSettings.MissionNames() is not empty: //see (*) above before changing
    
#if defined(IMPL_WIN)
    
	for ( auto const &mission : mSettings.MissionNames() )
	{
		src_missions += ( mRadsServerAddress + "/" + mission + " " );
	}
    
#else
    
    auto v = String2Vector( mRadsServerAddress, std::string( "::" ) );
    src_missions += v[0] + "::\"";
    for ( auto const &mission : mSettings.MissionNames() )
	{
		src_missions += ( v[1] + "/" + mission + " " );
	}
    src_missions.back() = '\"';

#endif    
    
    cmd_line += src_missions;
	cmd_line += " ";
	cmd_line += QuotePath( win2cygwin( mSettings.OutputDirectory() ) );

	const bool sync = false;
	mCurrentProcess = new COsProcess( sync, "", this, cmd_line );
	connect( mCurrentProcess, SIGNAL( readyReadStandardOutput() ),				this, SLOT( HandleUpdateOutput() ) );
	connect( mCurrentProcess, SIGNAL( readyReadStandardError() ),				this, SLOT( HandleUpdateOutput() ) );
	connect( mCurrentProcess, SIGNAL( finished( int, QProcess::ExitStatus ) ),	this, SLOT( HandleProcessFinished( int, QProcess::ExitStatus ) ) );
	connect( mCurrentProcess, SIGNAL( error( QProcess::ProcessError ) ),		this, SLOT( HandleRsyncError( QProcess::ProcessError ) ) );
	BroadcastRsyncStatusToSocketClients( RSYNC_RUNNING_SIGN.c_str() );
	mCurrentProcess->Execute();

	const QString msg = "A RADS data synchronization request was processed.\nCommand line: " + t2q( cmd_line );
	QtServiceBase::instance()->logMessage( msg );
	LOG_INFO( msg );
	DelaySaveConfig();

	return true;
}



//////////////////////////////////////////////////////////
//	RadsClient - rsync process handling
//////////////////////////////////////////////////////////


////////////////////////////////
// socket notifications handling
////////////////////////////////

void CRadsClient::HandleNewConnection()
{
	QLocalSocket *connection = mSocketServer->nextPendingConnection();
	mSocketConnections.push_back( connection );

	connect( connection, SIGNAL( disconnected() ), this, SLOT( HandleSocketDisconnected() ) );
	//connect( connection, SIGNAL( disconnected() ), connection, SLOT( deleteLater() ) );

	//new client needs immediate knowledge of current status
	//
	BroadcastRsyncStatusToSocketClients( mCurrentProcess ? RSYNC_RUNNING_SIGN.c_str() : RSYNC_STOPPED_SIGN.c_str() );

	LOG_WARN( "New socket connection received. " + connection->fullServerName() );
}


void CRadsClient::HandleSocketDisconnected()
{
	for ( auto it = mSocketConnections.begin(); it != mSocketConnections.end();  )
	{
		QLocalSocket *connection = *it;
		if ( connection->isValid() )
		{
			++it;
		}
		else
		{
			it = mSocketConnections.erase( it );	// it remains valid, pointing to next item
			connection->deleteLater();
			LOG_WARN( "Erased socket connection reference. " + connection->fullServerName() );
		}
	}
}


//slot
bool CRadsClient::BroadcastRsyncStatusToSocketClients( const char *msg )
{
	static bool test = false;

	if ( mSocketConnections.empty() )
		return true;

	if ( mBroadcasting )	//this should never happen; mBroadcasting is more a development device than a real sync. mechanism
	{
		QTimer::singleShot( 5000, this, [this, msg]() 
		{
			BroadcastRsyncStatusToSocketClients( msg );
		} );

		LOG_WARN( "Delaying socket notifications." );
		return false;
	}

	//test = !test;

	LOG_INFO( "Broadcasting status..." );

	mBroadcasting = true;

	QByteArray block;
	QDataStream out( &block, QIODevice::WriteOnly );
	out.setVersion( QDataStream::Qt_4_0 );
	out << (quint16)0;
	out << QString( msg );			   //QString, not raw char*, is critical
	out.device()->seek( 0 );
	out << (quint16)( block.size() - sizeof(quint16) );

	bool need_cleanup = false;					//apparently, a remote disconnection does not emit remotely...
	for ( auto *connection : mSocketConnections )
	{
		if ( !connection->isValid() )
		{
			need_cleanup = true;
			continue;
		}

		connection->write( block );
		connection->flush();
		//connection->disconnectFromServer();
	}

	if ( need_cleanup )
		QTimer::singleShot( 5000, this, &CRadsClient::HandleSocketDisconnected );

	mBroadcasting = false;

	LOG_INFO( "Finished broadcasting status." );

	return true;
}


//////////////////////
// cleanup helper
//////////////////////

void CRadsClient::CleanRsyncProcess( bool kill )		//kill = false 
{
	if ( mCurrentProcess )
	{
		if ( kill )
			mCurrentProcess->Kill();
		mCurrentProcess->deleteLater();
	}
	mCurrentProcess = nullptr;
	BroadcastRsyncStatusToSocketClients( RSYNC_STOPPED_SIGN.c_str() );
}


//////////////////////
// rsync process slots
//////////////////////

//slot
void CRadsClient::HandleRsyncError( QProcess::ProcessError error )
{
	auto message = "An error occurred launching " + mSettings.RadsPaths().mRsyncExecutablePath + "\n" + q2a( COsProcess::ProcessErrorMessage( error ) );
	QtServiceBase::instance()->logMessage( message.c_str() );
	LOG_FATAL( message.c_str() );
	CleanRsyncProcess();
}


//slot
void CRadsClient::HandleProcessFinished( int exit_code, QProcess::ExitStatus exitStatus )
{
    // data & lambdas: rsync exit codes translation mechanism 
    
    struct 
    {
        const int code;
        const std::string msg;        
    }  
    rsync_exit_codes[] =
    {
        0,      "Success",
        1,      "Syntax or usage error",
        2,      "Protocol incompatibility",
        3,      "Errors selecting input/output files, dirs",
        4,      "Requested  action not supported: an attempt was made to manipulate 64-bit files on a platform "
        "that cannot support them; or an option was specified that is supported by the client and not by the server.",
        5,      "Error starting client-server protocol",
        6,      "Daemon unable to append to log-file",
        10,     "Error in socket I/O",
        11,     "Error in file I/O",
        12,     "Error in rsync protocol data stream",
        13,     "Errors with program diagnostics",
        14,     "Error in IPC code",
        20,     "Received SIGUSR1 or SIGINT",
        21,     "Some error returned by waitpid()",
        22,     "Error allocating core memory buffers",
        23,     "Partial transfer due to error",
        24,     "Partial transfer due to vanished source files",
        25,     "The --max-delete limit stopped deletions",
        30,     "Timeout in data send/receive",
        35,      "Timeout waiting for daemon connection"
    };
    
    
    const DEFINE_ARRAY_SIZE( rsync_exit_codes );
    
    
    auto translate_code = [&rsync_exit_codes, &rsync_exit_codes_size](int code ) -> std::string
    {
        std::string s = n2s<std::string>( code );
        
        for ( int i = 0; i <  rsync_exit_codes_size; ++i )
            if ( rsync_exit_codes[i].code == code )
                return s += ( "-" + rsync_exit_codes[i].msg );
        
        return s;
    };
    
    
    // function body
    
    COsProcess *process = qobject_cast<COsProcess*>( sender() );	Q_UNUSED(process);		assert__( process && mCurrentProcess == process );

	if (exitStatus == QProcess::CrashExit) 
	{
		const QString msg = "program crash";
		QtServiceBase::instance()->logMessage( msg );
		LOG_FATAL( msg );
	} 
	else if ( exit_code != 0 )	//this is ExitStatus::NormalExit, although error code != 0
	{
		const QString msg = t2q( "exit code " + translate_code( exit_code ) );
		QtServiceBase::instance()->logMessage( msg );
		LOG_WARN( msg );
	} 
	else 
	{
        mSettings.SetLastSyncTimeNow();
		auto next = mSettings.NextSyncDate();
		const QString msg = 
            "A RADS data synchronization request was successfully processed.\nNext synchronization scheduled for "
            + next.toString( CRadsSettingsBase::RsyncDateFormat() );
        QtServiceBase::instance()->logMessage( msg );
        LOG_INFO( msg );
    
        DelaySaveConfig();
	}

	CleanRsyncProcess();
}


void CRadsClient::HandleUpdateOutput()
{
	COsProcess *process = qobject_cast<COsProcess*>( sender() );			assert__( process && mCurrentProcess == process );

	// log errors to system logs
	//
	QString text = process->readAllStandardError();
	if ( !text.isEmpty() )
	{
		QtServiceBase::instance()->logMessage( text );
		LOG_WARN( text );
	}

	//don't flood system logs with rsync output
	//
	text = process->readAllStandardOutput();
	if ( !text.isEmpty() )
	{
		LOG_INFO( text );
	}
}





//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//						RadsService
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

CRadsService::CRadsService( int argc, char *argv[], const CApplicationStaticPaths &paths, bool auto_start )
	: base_t( argc, argv, RADS_SERVICE_NAME )
	, mSettings( paths )
{
	LOG_INFO( "Creating a service object. Configuration file: " + mSettings.FilePath() );

	setServiceDescription( "Synchronizes data between RADS sever and local BRAT data repository" );
	setServiceFlags( QtServiceBase::CanBeSuspended );
	setStartupType( auto_start ? QtServiceController::StartupType::AutoStartup : QtServiceController::StartupType::ManualStartup );

	LOG_INFO( "Created a service object." );
}


//virtual 
CRadsService::~CRadsService()
{
	if ( mRadsClient )
	{
		delete mRadsClient;
		LOG_WARN( "Deleted rads client object in service destructor." );
	}
}


//virtual 
int CRadsService::exec()
{
	LOG_INFO( "Calling CRadsService::base_t::exec()" );
	return base_t::exec();
}

#ifndef _WIN32
    //#include <stdio.h>
    //#include <unistd.h>

    bool RunningUnderDebugger() //prevents debugger from attaching
    {
//        FILE *fd = fopen("/tmp", "r");
//        bool under = fileno(fd) > 5;
//        fclose(fd);
//        return under;
        return false;
    }
#else
    bool RunningUnderDebugger()
    {
         return IsDebuggerPresent() == TRUE;
    }
#endif


//virtual 
void CRadsService::start()
{

#if defined (WAIT_FOR_DEBUGGER)
#if defined (DEBUG) || defined (_DEBUG)
#if defined (_MSC_VER)
	LOG_INFO( "Waiting for debugger to attach..." );
    while ( !RunningUnderDebugger() ) QBratThread::sleep( 10 );
	LOG_INFO( "Debugger attached." );
#else
    //while ( !RunningUnderDebugger() ) QBratThread::sleep( 10 );
    QBratThread::sleep( 15 );
#endif
#endif
#endif

	bool config_result = false;
	if ( IsFile( mSettings.FilePath() ) )
		config_result = mSettings.LoadConfig();
	else
		config_result = mSettings.SaveConfig();		//save defaults for brat to read
	if ( !config_result )
		LOG_WARN( "An error occurred accessing the configuration file." );

	QCoreApplication *app = application();

	LOG_INFO( "Entering CRadsService::start()" );

	if ( mRadsClient )
	{
		delete mRadsClient;
		LOG_WARN( "Deleted rads client object before starting." );
	}

	mRadsClient = new CRadsClient( mSettings, app );

	if ( !mRadsClient->IsListening() )
	{
		const QString msg = "Failed to setup rads client. Quiting...";
		logMessage( msg, QtServiceBase::Error );
		LOG_FATAL( msg );
		app->quit();
		return;
	}

	LOG_INFO( "Leaving CRadsService::start() in expected return point." );
}


//virtual 
void CRadsService::stop()
{
	LOG_INFO( "Stop system command received." );
	delete mRadsClient;
	mRadsClient = nullptr;	
	base_t::stop();		//only because; it does nothing
}


//virtual 
void CRadsService::pause()
{
	QTimer::singleShot( 200, mRadsClient, &CRadsClient::Pause );
}


//virtual 
void CRadsService::resume()
{
	QTimer::singleShot( 200, mRadsClient, &CRadsClient::Resume );
}


//virtual 
void CRadsService::processCommand( int code )
{
	base_t::processCommand( code );			//just because; it does nothing

	switch ( code )
	{
		case eRadsService_ExecNow:
		{
			QTimer::singleShot( 200, mRadsClient, &CRadsClient::ForceSynchronize );
		}
		break;

		case eRadsService_LockConfig:
		{
			mSettings.LockFile( true );
		}
		break;

		case eRadsService_UnlockConfig:
		{
			if ( !mSettings.LockFile( false ) )	//reloads parameters
                LOG_WARN( "Error reading RADS service configuration while processing the unlock command." );
		}
		break;

		default:
		{
			LOG_WARN( "The service received an INAVALID command code." );
		}
	}
}


//virtual 
void CRadsService::logMessage( const QString &message, MessageType type, int id, uint category, const QByteArray &data )		//type = Success, int id = 0, uint category = 0, const QByteArray &data = QByteArray()
{
	base_t::logMessage( message, type, id, category, data );
	LOG_INFO( "Service framework logMessage: " + message );
}




//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//						main
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

//#define LOCAL_DEBUG



int main( int argc, char *argv[] )
{
#if defined (LOCAL_DEBUG)	

	qDebug() << "Decrypted rads_password to" << decrypt( "AwKel0QzUiZmEkIw", 89473829 );				//AwKqGyUhayqpheog or AwIfrpCU3p8cMF+V with default_key?
	qDebug() << "Encrypted rads_password to" << encrypt( "rat@tu1", 89473829 );
	qDebug() << "Encrypted rads_password to" << encrypt( "rat@tu1", 89473829 );
	QString to_decrypt = encrypt( "rat@tu1", 89473829 );				//AwKqGyUhayqpheog with default_key
	qDebug() << "Encrypted rads_password to" << to_decrypt;
	qDebug() << "Decrypted rads_password to" << decrypt( to_decrypt, 89473829 );

#if !defined(Q_OS_WIN)
    // QtService stores service settings in SystemScope, which normally require root privileges.
    // To allow testing this example as non-root, we change the directory of the SystemScope settings file.
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, QDir::tempPath());
    qWarning("(Example uses dummy settings file: %s/QtSoftware.conf)", QDir::tempPath().toLatin1().constData());
#endif
#endif

	// I. Application Paths

	const CApplicationStaticPaths brat_paths( argv[ 0 ], RADS_SERVICE_NAME );


	// II. Log files management

	CheckServiceLogging( brat_paths );

	LOG_INFO( "" );		LOG_INFO( " *** Logging started *** " );


#if defined (LOCAL_DEBUG)	
	QCoreApplication service( argc, argv );
	new CRadsClient( brat_paths );
	return service.exec();
#else


	// III. Service

	CRadsService service( argc, argv, brat_paths, true );		//auto start
	return service.exec();

#endif
}




///////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////

#include "moc_RadsService.cpp"


//QProcess *p = new QProcess;

//QString s = "launchctl load -w ~/Library/LaunchAgents/com.me.myApp.plist";

//p->start("bash", QStringList()<< "-c"<< s);
//qt code for launchctl command worked. But now the problem is that it creates the 2nd instance of my applications.

//com.me.myApp.plist file is stored in ~/Library/LaunchAgents.

//This plist file contains:

//Label - com.me.myApp
//RunAtLoad - True
//WorkingDirectory - /Applications/myApp
//Program - /Applications/myApp/Contents/MacOS/myApp
//KeepAlive - False
//Disabled - False