//Programmer: Brandon Allen
//Light Display Utility
//Version: 1.0

//Description: Reads in specified "show" files (ending in .show or .s, though any file extension can be used) and interprets the contents in real time to
//control up to 64 individually specifiable "channels" of Christmas light strands (or any other appliance that plugs into a standard 110V American power outlet).
//In the event that all 64 channels are to be implemented, GPIO pins 0-7 of the Raspberry Pi would be connected to the inputs of 8 octal D latches (because the Pi does
//not have enough GPIO pins to control every channel directly), pins 21-23 would be connected to the inputs of a 3-8 decoder, and pin 24 would connected to the
//decoder's enable (assuming everything is active HIGH). Each output of the decoder would then be connected to the clock of one of the octal D latches, and the
//outputs of the octal D latches would each be connected to a "set" of 8 solid state power relays. At some point this system will likely be modified to use
//D flip flops instead of latches(the only reason latches were used is because the original design used Pulse Width Modulation (PWM) to dim the lights, but the relays were
//not fast enough to keep up). Note: Channel numbering starts at 1 (not 0) in show files.

//Hardware Limitations(as of 6-15-2016): No more than 2 Amps can be pulled through any one channel. Current through all channels must add up to less than 15 Amps.

//--------------------------------------------------------------------SHOW FILE INFORMATION------------------------------------------------------------------------
//Show File Specification:
//[time] [command] [command parameters]

//[time]== Floating point number of seconds since audio playback began; Defines the time at which the specified command is to be executed.
//[command]== Instruction to execute at the given time. Possible commands are: play, on, off, mirror, unmirror, add, restart, end. See further descriptions below.
//[command parameters]== Information required to execute the given command. See command implementation below for specific parameters required.

//Show file must start with:
//0 play filename.mp3
//And end with "end", "restart", or "continue" command.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------

#ifndef LIGHT_DISPLAY_H
#define LIGHT_DISPLAY_H

class light_display
{
	private:
		const int MAX_CHANNELS=64;
		int currentChannels;//Number of channels currently employed by the Light Display System; MUST be less than MAX_CHANNELS.
	    const float CORRECTION=0;//Time correction associated with an inherent delay in audio playback (if a significant one is found to exist).
		const float CORRECTION2=0;//Time correction associated with an inherent delay in audio time stamp output by mpg123 decoder (if one is found to exist).
		ifstream fin;//input stream for show file.
		fstream log;//Output stream for log file.
		const string LOG_NAME="log.txt";
		char str[10]="start";//Stores current command value once it's read in from the show file.
		char song[30];//Stores mp3 filename.
		string filename;//Stores show file filename.
		double initial_time=0.0;//Stores starting number of clock cycles.
		double startTime=0.0;//Stores current command execution time (seconds since audio playback began; a.k.a. "song time") once it's read in from show file.
		double currTime=0.0;//Stores current number of seconds since audio playback began (excluding pauses).
		double prevTime=0.0;//Stores last known time determined from getTime().
		int channel;
		int level;
		int setchan[MAX_CHANNELS];//Keeps track of channel states.
		thread music;//Separate thread to play mp3 file on.
		double add=0.0;//Value to shift command execution times from show file.
		double temp_add=0.0;
		int mirror[MAX_CHANNELS];//Keeps track of mirrors on each channel.
		int unmirror=0;
		double initTime;//Value of getTime() before audio playback begins (leftover from previous audio playback).
		int line=0;//Keeps track of lines read in from show file.
		size_t found;
		bool restart=false;
		
	public:
		light_display(int channels);
		void initializeChannels();
		void initializeLog();
		void logMessage(string message);
		string timeStamp();
		void reset();
		void executeCommand();
		void runDisplay(string show="");
		
		//Description: Retrieves time from time.txt, which is the location at which mpg123 outputs the current audio playback time in seconds.
		//Preconditions: None.
		//Post-conditions: Returns the time specified in time.txt or returns -1 if time.txt could not be opened.
		double getTime();

		//Description: Uses system time and date to determine when it's night and wait until then if it's day.
		//Preconditions: Accurate system time and date, and approximately the same latitude as Kansas City, MO.
		//Post-condition: Waits until it's night if it's not already night.
		void nightWait();

		//Description: Uses mpg123 to play filename[] via a system command. Specifies for mpg123 to use a 1MB buffer, play blank space at ends of mp3 file,
		//output playback information, and allow keyboard controls (space to pause).
		//Precondition: filename[] is an mp3 file in the same directory this is executing in.
		//Post-condition: Thread that this is executed in will be halted until audio playback ends.
		void playsong(char filename[]);

		//Description: Changes the output values of the pins being used by the Light Display System in accordance with its hardware configuration.
		//Preconditions: channel must be between 1 and MAX_CHANNELS (inclusively), level must be 0 or 1.
		//Mirror values MUST NOT be recursive in that, channel a is mirrored to channel b and b is mirrored to a. channel_setter calls itself recursively for mirrored channels.
		//Post-condition: GPIO pins 0-7 and 21-24 may have changed output values.
		void channel_setter(int channel, int level);
};

//0, 1, 2, 3, 4, 5, 6, 7 for latch inputs
//21, 22, 23 for decoder inputs
//24 for decoder enable
//Channel numbering starts at 1, not 0.

#endif