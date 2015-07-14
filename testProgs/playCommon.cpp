/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2012, Live Networks, Inc.  All rights reserved
// A common framework, used for the "openRTSP" and "playSIP" applications
// Implementation
//
// NOTE: If you want to develop your own RTSP client application (or embed RTSP client functionality into your own application),
// then we don't recommend using this code as a model, because it is too complex (with many options).
// Instead, we recommend using the "testRTSPClient" application code as a model.

#include "playCommon.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"

#if defined(__WIN32__) || defined(_WIN32)
#define snprintf _snprintf
#else
#include <signal.h>
#define USE_SIGNALS 1
#endif

// Forward function definitions:
void continueAfterOPTIONS(RTSPClient* client, int resultCode, char* resultString);
void continueAfterDESCRIBE(RTSPClient* client, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* client, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* client, int resultCode, char* resultString);
void continueAfterTEARDOWN(RTSPClient* client, int resultCode, char* resultString);

void setupStreams();
void closeMediaSinks();
void subsessionAfterPlaying(void* clientData);
void subsessionByeHandler(void* clientData);
void sessionAfterPlaying(void* clientData = NULL);
void sessionTimerHandler(void* clientData);
void shutdown(int exitCode = 1);
void signalHandlerShutdown(int sig);
void checkForPacketArrival(void* clientData);
void checkInterPacketGaps(void* clientData);
void beginQOSMeasurement();

char const* progName;
UsageEnvironment* env;
Medium* ourClient = NULL;
Authenticator* ourAuthenticator = NULL;
char const* streamURL = NULL;
MediaSession* session = NULL;
TaskToken sessionTimerTask = NULL;
TaskToken arrivalCheckTimerTask = NULL;
TaskToken interPacketGapCheckTimerTask = NULL;
TaskToken qosMeasurementTimerTask = NULL;
Boolean createReceivers = True;
Boolean outputQuickTimeFile = False;
Boolean generateMP4Format = False;
QuickTimeFileSink* qtOut = NULL;
Boolean outputAVIFile = False;
AVIFileSink* aviOut = NULL;
Boolean audioOnly = False;
Boolean videoOnly = False;
char const* singleMedium = NULL;
int verbosityLevel = 1; // by default, print verbose output
double duration = 0;
double durationSlop = -1.0; // extra seconds to play at the end
double initialSeekTime = 0.0f;
float scale = 1.0f;
double endTime;
unsigned interPacketGapMaxTime = 0;
unsigned totNumPacketsReceived = ~0; // used if checking inter-packet gaps
Boolean playContinuously = False;
int simpleRTPoffsetArg = -1;
Boolean sendOptionsRequest = True;
Boolean sendOptionsRequestOnly = False;
Boolean oneFilePerFrame = False;
Boolean notifyOnPacketArrival = False;
Boolean streamUsingTCP = False;
unsigned short desiredPortNum = 0;
portNumBits tunnelOverHTTPPortNum = 0;
char* username = NULL;
char* password = NULL;
char* proxyServerName = NULL;
unsigned short proxyServerPortNum = 0;
unsigned char desiredAudioRTPPayloadFormat = 0;
char* mimeSubtype = NULL;
unsigned short movieWidth = 240; // default
Boolean movieWidthOptionSet = False;
unsigned short movieHeight = 180; // default
Boolean movieHeightOptionSet = False;
unsigned movieFPS = 15; // default
Boolean movieFPSOptionSet = False;
char const* fileNamePrefix = "";
unsigned fileSinkBufferSize = 100000;
unsigned socketInputBufferSize = 0;
Boolean packetLossCompensate = False;
Boolean syncStreams = False;
Boolean generateHintTracks = False;
unsigned qosMeasurementIntervalMS = 0; // 0 means: Don't output QOS data

struct timeval startTime;

void usage() {
  *env << "Usage: " << progName
       << " [-p <startPortNum>] [-r|-q|-4|-i] [-a|-v] [-V] [-d <duration>] [-D <max-inter-packet-gap-time> [-c] [-S <offset>] [-n] [-O]"
	   << (controlConnectionUsesTCP ? " [-t|-T <http-port>]" : "")
       << " [-u <username> <password>"
	   << (allowProxyServers ? " [<proxy-server> [<proxy-server-port>]]" : "")
       << "]" << (supportCodecSelection ? " [-A <audio-codec-rtp-payload-format-code>|-M <mime-subtype-name>]" : "")
       << " [-s <initial-seek-time>] [-z <scale>]"
       << " [-w <width> -h <height>] [-f <frames-per-second>] [-y] [-H] [-Q [<measurement-interval>]] [-F <filename-prefix>] [-b <file-sink-buffer-size>] [-B <input-socket-buffer-size>] [-I <input-interface-ip-address>] [-m] <url> (or " << progName << " -o [-V] <url>)\n";
  shutdown();
}

int main(int argc, char** argv) {
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  progName = argv[0];

  gettimeofday(&startTime, NULL);

#ifdef USE_SIGNALS
  // Allow ourselves to be shut down gracefully by a SIGHUP or a SIGUSR1:
  signal(SIGHUP, signalHandlerShutdown);
  signal(SIGUSR1, signalHandlerShutdown);
#endif

  // unfortunately we can't use getopt() here, as Windoze doesn't have it
  while (argc > 2) {
    char* const opt = argv[1];
    if (opt[0] != '-') usage();
    switch (opt[1]) {

    case 'p': { // specify start port number
      int portArg;
      if (sscanf(argv[2], "%d", &portArg) != 1) {
	usage();
      }
      if (portArg <= 0 || portArg >= 65536 || portArg&1) {
	*env << "bad port number: " << portArg
		<< " (must be even, and in the range (0,65536))\n";
	usage();
      }
      desiredPortNum = (unsigned short)portArg;
      ++argv; --argc;
      break;
    }

    case 'r': { // do not receive data (instead, just 'play' the stream(s))
      createReceivers = False;
      break;
    }

    case 'q': { // output a QuickTime file (to stdout)
      outputQuickTimeFile = True;
      break;
    }

    case '4': { // output a 'mp4'-format file (to stdout)
      outputQuickTimeFile = True;
      generateMP4Format = True;
      break;
    }

    case 'i': { // output an AVI file (to stdout)
      outputAVIFile = True;
      break;
    }

    case 'I': { // specify input interface...
      NetAddressList addresses(argv[2]);
      if (addresses.numAddresses() == 0) {
	*env << "Failed to find network address for \"" << argv[2] << "\"";
	break;
      }
      ReceivingInterfaceAddr = *(unsigned*)(addresses.firstAddress()->data());
      ++argv; --argc;
      break;
    }

    case 'a': { // receive/record an audio stream only
      audioOnly = True;
      singleMedium = "audio";
      break;
    }

    case 'v': { // receive/record a video stream only
      videoOnly = True;
      singleMedium = "video";
      break;
    }

    case 'V': { // disable verbose output
      verbosityLevel = 0;
      break;
    }

    case 'd': { // specify duration, or how much to delay after end time
      float arg;
      if (sscanf(argv[2], "%g", &arg) != 1) {
	usage();
      }
      if (argv[2][0] == '-') { // not "arg<0", in case argv[2] was "-0"
	// a 'negative' argument was specified; use this for "durationSlop":
	duration = 0; // use whatever's in the SDP
	durationSlop = -arg;
      } else {
	duration = arg;
	durationSlop = 0;
      }
      ++argv; --argc;
      break;
    }

    case 'D': { // specify maximum number of seconds to wait for packets:
      if (sscanf(argv[2], "%u", &interPacketGapMaxTime) != 1) {
	usage();
      }
      ++argv; --argc;
      break;
    }

    case 'c': { // play continuously
      playContinuously = True;
      break;
    }

    case 'S': { // specify an offset to use with "SimpleRTPSource"s
      if (sscanf(argv[2], "%d", &simpleRTPoffsetArg) != 1) {
	usage();
      }
      if (simpleRTPoffsetArg < 0) {
	*env << "offset argument to \"-S\" must be >= 0\n";
	usage();
      }
      ++argv; --argc;
      break;
    }

    case 'O': { // Don't send an "OPTIONS" request before "DESCRIBE"
      sendOptionsRequest = False;
      break;
    }

    case 'o': { // Send only the "OPTIONS" request to the server
      sendOptionsRequestOnly = True;
      break;
    }

    case 'm': { // output multiple files - one for each frame
      oneFilePerFrame = True;
      break;
    }

    case 'n': { // notify the user when the first data packet arrives
      notifyOnPacketArrival = True;
      break;
    }

    case 't': {
      // stream RTP and RTCP over the TCP 'control' connection
      if (controlConnectionUsesTCP) {
	streamUsingTCP = True;
      } else {
	usage();
      }
      break;
    }

    case 'T': {
      // stream RTP and RTCP over a HTTP connection
      if (controlConnectionUsesTCP) {
	if (argc > 3 && argv[2][0] != '-') {
	  // The next argument is the HTTP server port number:
	  if (sscanf(argv[2], "%hu", &tunnelOverHTTPPortNum) == 1
	      && tunnelOverHTTPPortNum > 0) {
	    ++argv; --argc;
	    break;
	  }
	}
      }

      // If we get here, the option was specified incorrectly:
      usage();
      break;
    }

    case 'u': { // specify a username and password
      username = argv[2];
      password = argv[3];
      argv+=2; argc-=2;
      if (allowProxyServers && argc > 3 && argv[2][0] != '-') {
	// The next argument is the name of a proxy server:
	proxyServerName = argv[2];
	++argv; --argc;

	if (argc > 3 && argv[2][0] != '-') {
	  // The next argument is the proxy server port number:
	  if (sscanf(argv[2], "%hu", &proxyServerPortNum) != 1) {
	    usage();
	  }
	  ++argv; --argc;
	}
      }

      ourAuthenticator = new Authenticator(username, password);
      break;
    }

    case 'A': { // specify a desired audio RTP payload format
      unsigned formatArg;
      if (sscanf(argv[2], "%u", &formatArg) != 1
	  || formatArg >= 96) {
	usage();
      }
      desiredAudioRTPPayloadFormat = (unsigned char)formatArg;
      ++argv; --argc;
      break;
    }

    case 'M': { // specify a MIME subtype for a dynamic RTP payload type
      mimeSubtype = argv[2];
      if (desiredAudioRTPPayloadFormat==0) desiredAudioRTPPayloadFormat =96;
      ++argv; --argc;
      break;
    }

    case 'w': { // specify a width (pixels) for an output QuickTime or AVI movie
      if (sscanf(argv[2], "%hu", &movieWidth) != 1) {
	usage();
      }
      movieWidthOptionSet = True;
      ++argv; --argc;
      break;
    }

    case 'h': { // specify a height (pixels) for an output QuickTime or AVI movie
      if (sscanf(argv[2], "%hu", &movieHeight) != 1) {
	usage();
      }
      movieHeightOptionSet = True;
      ++argv; --argc;
      break;
    }

    case 'f': { // specify a frame rate (per second) for an output QT or AVI movie
      if (sscanf(argv[2], "%u", &movieFPS) != 1) {
	usage();
      }
      movieFPSOptionSet = True;
      ++argv; --argc;
      break;
    }

    case 'F': { // specify a prefix for the audio and video output files
      fileNamePrefix = argv[2];
      ++argv; --argc;
      break;
    }

    case 'b': { // specify the size of buffers for "FileSink"s
      if (sscanf(argv[2], "%u", &fileSinkBufferSize) != 1) {
	usage();
      }
      ++argv; --argc;
      break;
    }

    case 'B': { // specify the size of input socket buffers
      if (sscanf(argv[2], "%u", &socketInputBufferSize) != 1) {
	usage();
      }
      ++argv; --argc;
      break;
    }

    // Note: The following option is deprecated, and may someday be removed:
    case 'l': { // try to compensate for packet loss by repeating frames
      packetLossCompensate = True;
      break;
    }

    case 'y': { // synchronize audio and video streams
      syncStreams = True;
      break;
    }

    case 'H': { // generate hint tracks (as well as the regular data tracks)
      generateHintTracks = True;
      break;
    }

    case 'Q': { // output QOS measurements
      qosMeasurementIntervalMS = 1000; // default: 1 second

      if (argc > 3 && argv[2][0] != '-') {
	// The next argument is the measurement interval,
	// in multiples of 100 ms
	if (sscanf(argv[2], "%u", &qosMeasurementIntervalMS) != 1) {
	  usage();
	}
	qosMeasurementIntervalMS *= 100;
	++argv; --argc;
      }
      break;
    }

    case 's': { // specify initial seek time (trick play)
      double arg;
      if (sscanf(argv[2], "%lg", &arg) != 1 || arg < 0) {
	usage();
      }
      initialSeekTime = arg;
      ++argv; --argc;
      break;
    }

    case 'z': { // scale (trick play)
      float arg;
      if (sscanf(argv[2], "%g", &arg) != 1 || arg == 0.0f) {
	usage();
      }
      scale = arg;
      ++argv; --argc;
      break;
    }

    default: {
      usage();
      break;
    }
    }

    ++argv; --argc;
  }
  if (argc != 2) usage();
  if (outputQuickTimeFile && outputAVIFile) {
    *env << "The -i and -q (or -4) flags cannot both be used!\n";
    usage();
  }
  Boolean outputCompositeFile = outputQuickTimeFile || outputAVIFile;
  if (!createReceivers && outputCompositeFile) {
    *env << "The -r and -q (or -4 or -i) flags cannot both be used!\n";
    usage();
  }
  if (outputCompositeFile && !movieWidthOptionSet) {
    *env << "Warning: The -q, -4 or -i option was used, but not -w.  Assuming a video width of "
	 << movieWidth << " pixels\n";
  }
  if (outputCompositeFile && !movieHeightOptionSet) {
    *env << "Warning: The -q, -4 or -i option was used, but not -h.  Assuming a video height of "
	 << movieHeight << " pixels\n";
  }
  if (outputCompositeFile && !movieFPSOptionSet) {
    *env << "Warning: The -q, -4 or -i option was used, but not -f.  Assuming a video frame rate of "
	 << movieFPS << " frames-per-second\n";
  }
  if (audioOnly && videoOnly) {
    *env << "The -a and -v flags cannot both be used!\n";
    usage();
  }
  if (sendOptionsRequestOnly && !sendOptionsRequest) {
    *env << "The -o and -O flags cannot both be used!\n";
    usage();
  }
  if (tunnelOverHTTPPortNum > 0) {
    if (streamUsingTCP) {
      *env << "The -t and -T flags cannot both be used!\n";
      usage();
    } else {
      streamUsingTCP = True;
    }
  }
  if (!createReceivers && notifyOnPacketArrival) {
    *env << "Warning: Because we're not receiving stream data, the -n flag has no effect\n";
  }
  if (durationSlop < 0) {
    // This parameter wasn't set, so use a default value.
    // If we're measuring QOS stats, then don't add any slop, to avoid
    // having 'empty' measurement intervals at the end.
    durationSlop = qosMeasurementIntervalMS > 0 ? 0.0 : 5.0;
  }

  streamURL = argv[1];

  // Create our client object:
  ourClient = createClient(*env, streamURL, verbosityLevel, progName);
  if (ourClient == NULL) {
    *env << "Failed to create " << clientProtocolName
		<< " client: " << env->getResultMsg() << "\n";
    shutdown();
  }

  if (sendOptionsRequest) {
    // Begin by sending an "OPTIONS" command:
    getOptions(continueAfterOPTIONS);
  } else {
    continueAfterOPTIONS(NULL, 0, NULL);
  }

  // All subsequent activity takes place within the event loop:
  env->taskScheduler().doEventLoop(); // does not return

  return 0; // only to prevent compiler warning
}

void continueAfterOPTIONS(RTSPClient*, int resultCode, char* resultString) {
  if (sendOptionsRequestOnly) {
    if (resultCode != 0) {
      *env << clientProtocolName << " \"OPTIONS\" request failed: " << resultString << "\n";
    } else {
      *env << clientProtocolName << " \"OPTIONS\" request returned: " << resultString << "\n";
    }
    shutdown();
  }
  delete[] resultString;

  // Next, get a SDP description for the stream:
  getSDPDescription(continueAfterDESCRIBE);
}

void continueAfterDESCRIBE(RTSPClient*, int resultCode, char* resultString) {
  if (resultCode != 0) {
    *env << "Failed to get a SDP description for the URL \"" << streamURL << "\": " << resultString << "\n";
    shutdown();
  }

  char* sdpDescription = resultString;
  *env << "Opened URL \"" << streamURL << "\", returning a SDP description:\n" << sdpDescription << "\n";

  // Create a media session object from this SDP description:
  session = MediaSession::createNew(*env, sdpDescription);
  delete[] sdpDescription;
  if (session == NULL) {
    *env << "Failed to create a MediaSession object from the SDP description: " << env->getResultMsg() << "\n";
    shutdown();
  } else if (!session->hasSubsessions()) {
    *env << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
    shutdown();
  }

  // Then, setup the "RTPSource"s for the session:
  MediaSubsessionIterator iter(*session);
  MediaSubsession *subsession;
  Boolean madeProgress = False;
  char const* singleMediumToTest = singleMedium;
  while ((subsession = iter.next()) != NULL) {
    // If we've asked to receive only a single medium, then check this now:
    if (singleMediumToTest != NULL) {
      if (strcmp(subsession->mediumName(), singleMediumToTest) != 0) {
		  *env << "Ignoring \"" << subsession->mediumName()
			  << "/" << subsession->codecName()
			  << "\" subsession, because we've asked to receive a single " << singleMedium
			  << " session only\n";
	continue;
      } else {
	// Receive this subsession only
	singleMediumToTest = "xxxxx";
	    // this hack ensures that we get only 1 subsession of this type
      }
    }

    if (desiredPortNum != 0) {
      subsession->setClientPortNum(desiredPortNum);
      desiredPortNum += 2;
    }

    if (createReceivers) {
      if (!subsession->initiate(simpleRTPoffsetArg)) {
	*env << "Unable to create receiver for \"" << subsession->mediumName()
	     << "/" << subsession->codecName()
	     << "\" subsession: " << env->getResultMsg() << "\n";
      } else {
	*env << "Created receiver for \"" << subsession->mediumName()
	     << "/" << subsession->codecName()
	     << "\" subsession (client ports " << subsession->clientPortNum()
	     << "-" << subsession->clientPortNum()+1 << ")\n";
	madeProgress = True;
	
	if (subsession->rtpSource() != NULL) {
	  // Because we're saving the incoming data, rather than playing
	  // it in real time, allow an especially large time threshold
	  // (1 second) for reordering misordered incoming packets:
	  unsigned const thresh = 1000000; // 1 second
	  subsession->rtpSource()->setPacketReorderingThresholdTime(thresh);
	  
	  // Set the RTP source's OS socket buffer size as appropriate - either if we were explicitly asked (using -B),
	  // or if the desired FileSink buffer size happens to be larger than the current OS socket buffer size.
	  // (The latter case is a heuristic, on the assumption that if the user asked for a large FileSink buffer size,
	  // then the input data rate may be large enough to justify increasing the OS socket buffer size also.)
	  int socketNum = subsession->rtpSource()->RTPgs()->socketNum();
	  unsigned curBufferSize = getReceiveBufferSize(*env, socketNum);
	  if (socketInputBufferSize > 0 || fileSinkBufferSize > curBufferSize) {
	    unsigned newBufferSize = socketInputBufferSize > 0 ? socketInputBufferSize : fileSinkBufferSize;
	    newBufferSize = setReceiveBufferTo(*env, socketNum, newBufferSize);
	    if (socketInputBufferSize > 0) { // The user explicitly asked for the new socket buffer size; announce it:
	      *env << "Changed socket receive buffer size for the \""
		   << subsession->mediumName()
		   << "/" << subsession->codecName()
		   << "\" subsession from "
		   << curBufferSize << " to "
		   << newBufferSize << " bytes\n";
	    }
	  }
	}
      }
    } else {
      if (subsession->clientPortNum() == 0) {
	*env << "No client port was specified for the \""
	     << subsession->mediumName()
	     << "/" << subsession->codecName()
	     << "\" subsession.  (Try adding the \"-p <portNum>\" option.)\n";
      } else {
		madeProgress = True;
      }
    }
  }
  if (!madeProgress) shutdown();

  // Perform additional 'setup' on each subsession, before playing them:
  setupStreams();
}

MediaSubsession *subsession;
Boolean madeProgress = False;
void continueAfterSETUP(RTSPClient*, int resultCode, char* resultString) {
  if (resultCode == 0) {
      *env << "Setup \"" << subsession->mediumName()
	   << "/" << subsession->codecName()
	   << "\" subsession (client ports " << subsession->clientPortNum()
	   << "-" << subsession->clientPortNum()+1 << ")\n";
      madeProgress = True;
  } else {
    *env << "Failed to setup \"" << subsession->mediumName()
	 << "/" << subsession->codecName()
	 << "\" subsession: " << env->getResultMsg() << "\n";
  }

  // Set up the next subsession, if any:
  setupStreams();
}

void setupStreams() {
  static MediaSubsessionIterator* setupIter = NULL;
  if (setupIter == NULL) setupIter = new MediaSubsessionIterator(*session);
  while ((subsession = setupIter->next()) != NULL) {
    // We have another subsession left to set up:
    if (subsession->clientPortNum() == 0) continue; // port # was not set

    setupSubsession(subsession, streamUsingTCP, continueAfterSETUP);
    return;
  }

  // We're done setting up subsessions.
  delete setupIter;
  if (!madeProgress) shutdown();

  // Create output files:
  if (createReceivers) {
    if (outputQuickTimeFile) {
      // Create a "QuickTimeFileSink", to write to 'stdout':
      qtOut = QuickTimeFileSink::createNew(*env, *session, "stdout",
					   fileSinkBufferSize,
					   movieWidth, movieHeight,
					   movieFPS,
					   packetLossCompensate,
					   syncStreams,
					   generateHintTracks,
					   generateMP4Format);
      if (qtOut == NULL) {
	*env << "Failed to create QuickTime file sink for stdout: " << env->getResultMsg();
	shutdown();
      }

      qtOut->startPlaying(sessionAfterPlaying, NULL);
    } else if (outputAVIFile) {
      // Create an "AVIFileSink", to write to 'stdout':
      aviOut = AVIFileSink::createNew(*env, *session, "stdout",
				      fileSinkBufferSize,
				      movieWidth, movieHeight,
				      movieFPS,
				      packetLossCompensate);
      if (aviOut == NULL) {
	*env << "Failed to create AVI file sink for stdout: " << env->getResultMsg();
	shutdown();
      }

      aviOut->startPlaying(sessionAfterPlaying, NULL);
    } else {
      // Create and start "FileSink"s for each subsession:
      madeProgress = False;
      MediaSubsessionIterator iter(*session);
      while ((subsession = iter.next()) != NULL) {
	if (subsession->readSource() == NULL) continue; // was not initiated

	// Create an output file for each desired stream:
	char outFileName[1000];
	if (singleMedium == NULL) {
	  // Output file name is
	  //     "<filename-prefix><medium_name>-<codec_name>-<counter>"
	  static unsigned streamCounter = 0;
	  snprintf(outFileName, sizeof outFileName, "%s%s-%s-%d",
		   fileNamePrefix, subsession->mediumName(),
		   subsession->codecName(), ++streamCounter);
	} else {
	  sprintf(outFileName, "stdout");
	}
	FileSink* fileSink;
	if (strcmp(subsession->mediumName(), "audio") == 0 &&
	    (strcmp(subsession->codecName(), "AMR") == 0 ||
	     strcmp(subsession->codecName(), "AMR-WB") == 0)) {
	  // For AMR audio streams, we use a special sink that inserts AMR frame hdrs:
	  fileSink = AMRAudioFileSink::createNew(*env, outFileName,
						 fileSinkBufferSize, oneFilePerFrame);
	} else if (strcmp(subsession->mediumName(), "video") == 0 &&
	    (strcmp(subsession->codecName(), "H264") == 0)) {
	  // For H.264 video stream, we use a special sink that insert start_codes:
	  fileSink = H264VideoFileSink::createNew(*env, outFileName,
						  subsession->fmtp_spropparametersets(),
						  fileSinkBufferSize, oneFilePerFrame);
	} else {
	  // Normal case:
	  fileSink = FileSink::createNew(*env, outFileName,
					 fileSinkBufferSize, oneFilePerFrame);
	}
	subsession->sink = fileSink;
	if (subsession->sink == NULL) {
	  *env << "Failed to create FileSink for \"" << outFileName
		  << "\": " << env->getResultMsg() << "\n";
	} else {
	  if (singleMedium == NULL) {
	    *env << "Created output file: \"" << outFileName << "\"\n";
	  } else {
	    *env << "Outputting data from the \"" << subsession->mediumName()
			<< "/" << subsession->codecName()
			<< "\" subsession to 'stdout'\n";
	  }

	  if (strcmp(subsession->mediumName(), "video") == 0 &&
	      strcmp(subsession->codecName(), "MP4V-ES") == 0 &&
	      subsession->fmtp_config() != NULL) {
	    // For MPEG-4 video RTP streams, the 'config' information
	    // from the SDP description contains useful VOL etc. headers.
	    // Insert this data at the front of the output file:
	    unsigned configLen;
	    unsigned char* configData
	      = parseGeneralConfigStr(subsession->fmtp_config(), configLen);
	    struct timeval timeNow;
	    gettimeofday(&timeNow, NULL);
	    fileSink->addData(configData, configLen, timeNow);
	    delete[] configData;
	  }

	  subsession->sink->startPlaying(*(subsession->readSource()),
					 subsessionAfterPlaying,
					 subsession);

	  // Also set a handler to be called if a RTCP "BYE" arrives
	  // for this subsession:
	  if (subsession->rtcpInstance() != NULL) {
	    subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, subsession);
	  }

	  madeProgress = True;
	}
      }
      if (!madeProgress) shutdown();
    }
  }

  // Finally, start playing each subsession, to start the data flow:
  if (duration == 0) {
    if (scale > 0) duration = session->playEndTime() - initialSeekTime; // use SDP end time
    else if (scale < 0) duration = initialSeekTime;
  }
  if (duration < 0) duration = 0.0;

  endTime = initialSeekTime;
  if (scale > 0) {
    if (duration <= 0) endTime = -1.0f;
    else endTime = initialSeekTime + duration;
  } else {
    endTime = initialSeekTime - duration;
    if (endTime < 0) endTime = 0.0f;
  }

  startPlayingSession(session, initialSeekTime, endTime, scale, continueAfterPLAY);
}

void continueAfterPLAY(RTSPClient*, int resultCode, char* resultString) {
  if (resultCode != 0) {
    *env << "Failed to start playing session: " << resultString << "\n";
    shutdown();
  } else {
    *env << "Started playing session\n";
  }

  if (qosMeasurementIntervalMS > 0) {
    // Begin periodic QOS measurements:
    beginQOSMeasurement();
  }

  // Figure out how long to delay (if at all) before shutting down, or
  // repeating the playing
  Boolean timerIsBeingUsed = False;
  double secondsToDelay = duration;
  if (duration > 0) {
    // First, adjust "duration" based on any change to the play range (that was specified in the "PLAY" response):
    double rangeAdjustment = (session->playEndTime() - session->playStartTime()) - (endTime - initialSeekTime);
    if (duration + rangeAdjustment > 0.0) duration += rangeAdjustment;

    timerIsBeingUsed = True;
    double absScale = scale > 0 ? scale : -scale; // ASSERT: scale != 0
    secondsToDelay = duration/absScale + durationSlop;

    int64_t uSecsToDelay = (int64_t)(secondsToDelay*1000000.0);
    sessionTimerTask = env->taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)sessionTimerHandler, (void*)NULL);
  }

  char const* actionString
    = createReceivers? "Receiving streamed data":"Data is being streamed";
  if (timerIsBeingUsed) {
    *env << actionString
		<< " (for up to " << secondsToDelay
		<< " seconds)...\n";
  } else {
#ifdef USE_SIGNALS
    pid_t ourPid = getpid();
    *env << actionString
		<< " (signal with \"kill -HUP " << (int)ourPid
		<< "\" or \"kill -USR1 " << (int)ourPid
		<< "\" to terminate)...\n";
#else
    *env << actionString << "...\n";
#endif
  }

  // Watch for incoming packets (if desired):
  checkForPacketArrival(NULL);
  checkInterPacketGaps(NULL);
}

void closeMediaSinks() {
  Medium::close(qtOut);
  Medium::close(aviOut);

  if (session == NULL) return;
  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != NULL) {
    Medium::close(subsession->sink);
    subsession->sink = NULL;
  }
}

void subsessionAfterPlaying(void* clientData) {
  // Begin by closing this media subsession's stream:
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL) return; // this subsession is still active
  }

  // All subsessions' streams have now been closed
  sessionAfterPlaying();
}

void subsessionByeHandler(void* clientData) {
  struct timeval timeNow;
  gettimeofday(&timeNow, NULL);
  unsigned secsDiff = timeNow.tv_sec - startTime.tv_sec;

  MediaSubsession* subsession = (MediaSubsession*)clientData;
  *env << "Received RTCP \"BYE\" on \"" << subsession->mediumName()
	<< "/" << subsession->codecName()
	<< "\" subsession (after " << secsDiff
	<< " seconds)\n";

  // Act now as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void sessionAfterPlaying(void* /*clientData*/) {
  if (!playContinuously) {
    shutdown(0);
  } else {
    // We've been asked to play the stream(s) over again.
    // First, reset state from the current session:
    if (env != NULL) {
      env->taskScheduler().unscheduleDelayedTask(sessionTimerTask);
      env->taskScheduler().unscheduleDelayedTask(arrivalCheckTimerTask);
      env->taskScheduler().unscheduleDelayedTask(interPacketGapCheckTimerTask);
      env->taskScheduler().unscheduleDelayedTask(qosMeasurementTimerTask);
    }
    totNumPacketsReceived = ~0;

    startPlayingSession(session, initialSeekTime, endTime, scale, continueAfterPLAY);
  }
}

void sessionTimerHandler(void* /*clientData*/) {
  sessionTimerTask = NULL;

  sessionAfterPlaying();
}

class qosMeasurementRecord {
public:
  qosMeasurementRecord(struct timeval const& startTime, RTPSource* src)
    : fSource(src), fNext(NULL),
      kbits_per_second_min(1e20), kbits_per_second_max(0),
      kBytesTotal(0.0),
      packet_loss_fraction_min(1.0), packet_loss_fraction_max(0.0),
      totNumPacketsReceived(0), totNumPacketsExpected(0) {
    measurementEndTime = measurementStartTime = startTime;

    RTPReceptionStatsDB::Iterator statsIter(src->receptionStatsDB());
    // Assume that there's only one SSRC source (usually the case):
    RTPReceptionStats* stats = statsIter.next(True);
    if (stats != NULL) {
      kBytesTotal = stats->totNumKBytesReceived();
      totNumPacketsReceived = stats->totNumPacketsReceived();
      totNumPacketsExpected = stats->totNumPacketsExpected();
    }
  }
  virtual ~qosMeasurementRecord() { delete fNext; }

  void periodicQOSMeasurement(struct timeval const& timeNow);

public:
  RTPSource* fSource;
  qosMeasurementRecord* fNext;

public:
  struct timeval measurementStartTime, measurementEndTime;
  double kbits_per_second_min, kbits_per_second_max;
  double kBytesTotal;
  double packet_loss_fraction_min, packet_loss_fraction_max;
  unsigned totNumPacketsReceived, totNumPacketsExpected;
};

static qosMeasurementRecord* qosRecordHead = NULL;

static void periodicQOSMeasurement(void* clientData); // forward

static unsigned nextQOSMeasurementUSecs;

static void scheduleNextQOSMeasurement() {
  nextQOSMeasurementUSecs += qosMeasurementIntervalMS*1000;
  struct timeval timeNow;
  gettimeofday(&timeNow, NULL);
  unsigned timeNowUSecs = timeNow.tv_sec*1000000 + timeNow.tv_usec;
  unsigned usecsToDelay = nextQOSMeasurementUSecs - timeNowUSecs;
     // Note: This works even when nextQOSMeasurementUSecs wraps around

  qosMeasurementTimerTask = env->taskScheduler().scheduleDelayedTask(
     usecsToDelay, (TaskFunc*)periodicQOSMeasurement, (void*)NULL);
}

static void periodicQOSMeasurement(void* /*clientData*/) {
  struct timeval timeNow;
  gettimeofday(&timeNow, NULL);

  for (qosMeasurementRecord* qosRecord = qosRecordHead;
       qosRecord != NULL; qosRecord = qosRecord->fNext) {
    qosRecord->periodicQOSMeasurement(timeNow);
  }

  // Do this again later:
  scheduleNextQOSMeasurement();
}

void qosMeasurementRecord
::periodicQOSMeasurement(struct timeval const& timeNow) {
  unsigned secsDiff = timeNow.tv_sec - measurementEndTime.tv_sec;
  int usecsDiff = timeNow.tv_usec - measurementEndTime.tv_usec;
  double timeDiff = secsDiff + usecsDiff/1000000.0;
  measurementEndTime = timeNow;

  RTPReceptionStatsDB::Iterator statsIter(fSource->receptionStatsDB());
  // Assume that there's only one SSRC source (usually the case):
  RTPReceptionStats* stats = statsIter.next(True);
  if (stats != NULL) {
    double kBytesTotalNow = stats->totNumKBytesReceived();
    double kBytesDeltaNow = kBytesTotalNow - kBytesTotal;
    kBytesTotal = kBytesTotalNow;

    double kbpsNow = timeDiff == 0.0 ? 0.0 : 8*kBytesDeltaNow/timeDiff;
    if (kbpsNow < 0.0) kbpsNow = 0.0; // in case of roundoff error
    if (kbpsNow < kbits_per_second_min) kbits_per_second_min = kbpsNow;
    if (kbpsNow > kbits_per_second_max) kbits_per_second_max = kbpsNow;

    unsigned totReceivedNow = stats->totNumPacketsReceived();
    unsigned totExpectedNow = stats->totNumPacketsExpected();
    unsigned deltaReceivedNow = totReceivedNow - totNumPacketsReceived;
    unsigned deltaExpectedNow = totExpectedNow - totNumPacketsExpected;
    totNumPacketsReceived = totReceivedNow;
    totNumPacketsExpected = totExpectedNow;

    double lossFractionNow = deltaExpectedNow == 0 ? 0.0
      : 1.0 - deltaReceivedNow/(double)deltaExpectedNow;
    //if (lossFractionNow < 0.0) lossFractionNow = 0.0; //reordering can cause
    if (lossFractionNow < packet_loss_fraction_min) {
      packet_loss_fraction_min = lossFractionNow;
    }
    if (lossFractionNow > packet_loss_fraction_max) {
      packet_loss_fraction_max = lossFractionNow;
    }
  }
}

void beginQOSMeasurement() {
  // Set up a measurement record for each active subsession:
  struct timeval startTime;
  gettimeofday(&startTime, NULL);
  nextQOSMeasurementUSecs = startTime.tv_sec*1000000 + startTime.tv_usec;
  qosMeasurementRecord* qosRecordTail = NULL;
  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != NULL) {
    RTPSource* src = subsession->rtpSource();
    if (src == NULL) continue;

    qosMeasurementRecord* qosRecord
      = new qosMeasurementRecord(startTime, src);
    if (qosRecordHead == NULL) qosRecordHead = qosRecord;
    if (qosRecordTail != NULL) qosRecordTail->fNext = qosRecord;
    qosRecordTail  = qosRecord;
  }

  // Then schedule the first of the periodic measurements:
  scheduleNextQOSMeasurement();
}

void printQOSData(int exitCode) {
  *env << "begin_QOS_statistics\n";
  
  // Print out stats for each active subsession:
  qosMeasurementRecord* curQOSRecord = qosRecordHead;
  if (session != NULL) {
    MediaSubsessionIterator iter(*session);
    MediaSubsession* subsession;
    while ((subsession = iter.next()) != NULL) {
      RTPSource* src = subsession->rtpSource();
      if (src == NULL) continue;
      
      *env << "subsession\t" << subsession->mediumName()
	   << "/" << subsession->codecName() << "\n";
      
      unsigned numPacketsReceived = 0, numPacketsExpected = 0;
      
      if (curQOSRecord != NULL) {
	numPacketsReceived = curQOSRecord->totNumPacketsReceived;
	numPacketsExpected = curQOSRecord->totNumPacketsExpected;
      }
      *env << "num_packets_received\t" << numPacketsReceived << "\n";
      *env << "num_packets_lost\t" << int(numPacketsExpected - numPacketsReceived) << "\n";
      
      if (curQOSRecord != NULL) {
	unsigned secsDiff = curQOSRecord->measurementEndTime.tv_sec
	  - curQOSRecord->measurementStartTime.tv_sec;
	int usecsDiff = curQOSRecord->measurementEndTime.tv_usec
	  - curQOSRecord->measurementStartTime.tv_usec;
	double measurementTime = secsDiff + usecsDiff/1000000.0;
	*env << "elapsed_measurement_time\t" << measurementTime << "\n";
	
	*env << "kBytes_received_total\t" << curQOSRecord->kBytesTotal << "\n";
	
	*env << "measurement_sampling_interval_ms\t" << qosMeasurementIntervalMS << "\n";
	
	if (curQOSRecord->kbits_per_second_max == 0) {
	  // special case: we didn't receive any data:
	  *env <<
	    "kbits_per_second_min\tunavailable\n"
	    "kbits_per_second_ave\tunavailable\n"
	    "kbits_per_second_max\tunavailable\n";
	} else {
	  *env << "kbits_per_second_min\t" << curQOSRecord->kbits_per_second_min << "\n";
	  *env << "kbits_per_second_ave\t"
	       << (measurementTime == 0.0 ? 0.0 : 8*curQOSRecord->kBytesTotal/measurementTime) << "\n";
	  *env << "kbits_per_second_max\t" << curQOSRecord->kbits_per_second_max << "\n";
	}
	
	*env << "packet_loss_percentage_min\t" << 100*curQOSRecord->packet_loss_fraction_min << "\n";
	double packetLossFraction = numPacketsExpected == 0 ? 1.0
	  : 1.0 - numPacketsReceived/(double)numPacketsExpected;
	if (packetLossFraction < 0.0) packetLossFraction = 0.0;
	*env << "packet_loss_percentage_ave\t" << 100*packetLossFraction << "\n";
	*env << "packet_loss_percentage_max\t"
	     << (packetLossFraction == 1.0 ? 100.0 : 100*curQOSRecord->packet_loss_fraction_max) << "\n";
	
	RTPReceptionStatsDB::Iterator statsIter(src->receptionStatsDB());
	// Assume that there's only one SSRC source (usually the case):
	RTPReceptionStats* stats = statsIter.next(True);
	if (stats != NULL) {
	  *env << "inter_packet_gap_ms_min\t" << stats->minInterPacketGapUS()/1000.0 << "\n";
	  struct timeval totalGaps = stats->totalInterPacketGaps();
	  double totalGapsMS = totalGaps.tv_sec*1000.0 + totalGaps.tv_usec/1000.0;
	  unsigned totNumPacketsReceived = stats->totNumPacketsReceived();
	  *env << "inter_packet_gap_ms_ave\t"
	       << (totNumPacketsReceived == 0 ? 0.0 : totalGapsMS/totNumPacketsReceived) << "\n";
	  *env << "inter_packet_gap_ms_max\t" << stats->maxInterPacketGapUS()/1000.0 << "\n";
	}
	
	curQOSRecord = curQOSRecord->fNext;
      }
    }
  }

  *env << "end_QOS_statistics\n";
  delete qosRecordHead;
}

Boolean areAlreadyShuttingDown = False;
int shutdownExitCode;
void shutdown(int exitCode) {
  if (areAlreadyShuttingDown) return; // in case we're called after receiving a RTCP "BYE" while in the middle of a "TEARDOWN".
  areAlreadyShuttingDown = True;

  shutdownExitCode = exitCode;
  if (env != NULL) {
    env->taskScheduler().unscheduleDelayedTask(sessionTimerTask);
    env->taskScheduler().unscheduleDelayedTask(arrivalCheckTimerTask);
    env->taskScheduler().unscheduleDelayedTask(interPacketGapCheckTimerTask);
    env->taskScheduler().unscheduleDelayedTask(qosMeasurementTimerTask);
  }

  if (qosMeasurementIntervalMS > 0) {
    printQOSData(exitCode);
  }

  // Teardown, then shutdown, any outstanding RTP/RTCP subsessions
  if (session != NULL) {
    tearDownSession(session, continueAfterTEARDOWN);
  } else {
    continueAfterTEARDOWN(NULL, 0, NULL);
  }
}

void continueAfterTEARDOWN(RTSPClient*, int /*resultCode*/, char* /*resultString*/) {
  // Now that we've stopped any more incoming data from arriving, close our output files:
  closeMediaSinks();
  Medium::close(session);

  // Finally, shut down our client:
  delete ourAuthenticator;
  Medium::close(ourClient);

  // Adios...
  exit(shutdownExitCode);
}

void signalHandlerShutdown(int /*sig*/) {
  *env << "Got shutdown signal\n";
  shutdown(0);
}

void checkForPacketArrival(void* /*clientData*/) {
  if (!notifyOnPacketArrival) return; // we're not checking

  // Check each subsession, to see whether it has received data packets:
  unsigned numSubsessionsChecked = 0;
  unsigned numSubsessionsWithReceivedData = 0;
  unsigned numSubsessionsThatHaveBeenSynced = 0;

  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != NULL) {
    RTPSource* src = subsession->rtpSource();
    if (src == NULL) continue;
    ++numSubsessionsChecked;

    if (src->receptionStatsDB().numActiveSourcesSinceLastReset() > 0) {
      // At least one data packet has arrived
      ++numSubsessionsWithReceivedData;
    }
    if (src->hasBeenSynchronizedUsingRTCP()) {
      ++numSubsessionsThatHaveBeenSynced;
    }
  }

  unsigned numSubsessionsToCheck = numSubsessionsChecked;
  // Special case for "QuickTimeFileSink"s and "AVIFileSink"s:
  // They might not use all of the input sources:
  if (qtOut != NULL) {
    numSubsessionsToCheck = qtOut->numActiveSubsessions();
  } else if (aviOut != NULL) {
    numSubsessionsToCheck = aviOut->numActiveSubsessions();
  }

  Boolean notifyTheUser;
  if (!syncStreams) {
    notifyTheUser = numSubsessionsWithReceivedData > 0; // easy case
  } else {
    notifyTheUser = numSubsessionsWithReceivedData >= numSubsessionsToCheck
      && numSubsessionsThatHaveBeenSynced == numSubsessionsChecked;
    // Note: A subsession with no active sources is considered to be synced
  }
  if (notifyTheUser) {
    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);
	char timestampStr[100];
	sprintf(timestampStr, "%ld%03ld", timeNow.tv_sec, (long)(timeNow.tv_usec/1000));
    *env << (syncStreams ? "Synchronized d" : "D")
		<< "ata packets have begun arriving [" << timestampStr << "]\007\n";
    return;
  }

  // No luck, so reschedule this check again, after a delay:
  int uSecsToDelay = 100000; // 100 ms
  arrivalCheckTimerTask
    = env->taskScheduler().scheduleDelayedTask(uSecsToDelay,
			       (TaskFunc*)checkForPacketArrival, NULL);
}

void checkInterPacketGaps(void* /*clientData*/) {
  if (interPacketGapMaxTime == 0) return; // we're not checking

  // Check each subsession, counting up how many packets have been received:
  unsigned newTotNumPacketsReceived = 0;

  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != NULL) {
    RTPSource* src = subsession->rtpSource();
    if (src == NULL) continue;
    newTotNumPacketsReceived += src->receptionStatsDB().totNumPacketsReceived();
  }

  if (newTotNumPacketsReceived == totNumPacketsReceived) {
    // No additional packets have been received since the last time we
    // checked, so end this stream:
    *env << "Closing session, because we stopped receiving packets.\n";
    interPacketGapCheckTimerTask = NULL;
    sessionAfterPlaying();
  } else {
    totNumPacketsReceived = newTotNumPacketsReceived;
    // Check again, after the specified delay:
    interPacketGapCheckTimerTask
      = env->taskScheduler().scheduleDelayedTask(interPacketGapMaxTime*1000000,
				 (TaskFunc*)checkInterPacketGaps, NULL);
  }
}
