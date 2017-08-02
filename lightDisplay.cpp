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
//And end with "end" command or "restart" command.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------

using namespace std;

#include <iostream>
#include <ctime>
#include <time.h>
#include <wiringPi.h>
#include <fstream>
#include <string.h>
#include <thread>
#include <stdlib.h>

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
//Preconditions: channel must be between 1 and MAX_CHANNELS (inclusively), level must be 0 or 1, setchan and mirror must be arrays of MAX_CHANNELS size.
//Mirror values MUST NOT be recursive in that, channel a is mirrored to channel b and b is mirrored to a. channel_setter calls itself recursively for mirrored channels.
//Post-condition: GPIO pins 0-7 and 21-24 may have changed output values.
void channel_setter(int channel, int level, int *setchan, int *mirror);

const int MAX_CHANNELS=8;//Number of channels currently employed by the Light Display System; MUST be 64 or less.

int main(int argc, char *argv[])
{
    const float CORRECTION=0;//Time correction associated with an inherent delay in audio playback (if a significant one is found to exist).
    const float CORRECTION2=0;//Time correction associated with an inherent delay in audio time stamp output by mpg123 decoder (if one is found to exist).
    ifstream fin;//input stream for show file.
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
    //Initialize GPIO ports
    wiringPiSetup();
    pinMode(21, OUTPUT);
    pinMode(22, OUTPUT);
    pinMode(23, OUTPUT);
    pinMode(24, OUTPUT);
    pinMode(0, OUTPUT);
    pinMode(1, OUTPUT);
    pinMode(2, OUTPUT);
    pinMode(3, OUTPUT);
    pinMode(4, OUTPUT);
    pinMode(5, OUTPUT);
    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);

    bool restart=false;
    while(1)
    {
        for(int i=0; i<MAX_CHANNELS; i++)
        {
            mirror[i]=0;
            setchan[i]=0;
        }
        initTime=getTime();
        initial_time=0;
        line=0;
        add=0;
        currTime=0.0;
        strcpy(str, "start");
        prevTime=getTime();
        if(restart==false&&argc<2)//This block of code must be skipped if a "restart" command was used in the last show file execution.
        {
            cout<<"Type in the name of the show file you would like to play: ";
            cin>>filename;
            //If no file extension is specified, ".show" is automatically appended to the end of the given filename.
            found=filename.find('.');
            if (found==string::npos)
                filename.append(".show");
        }
        else if (argc>=2)
	{
	    filename=argv[1];
            //If no file extension is specified, ".show" is automatically appended to the end of the given filename.
            found=filename.find('.');
            if (found==string::npos)
                filename.append(".show");
	}
        restart=false;
        fin.open(filename.c_str());
        if (fin.is_open())//read show file
        {
            while(strcmp(str, "end"))//"end" ends the loop (ALL COMMANDS ARE PRECEEDED BY A START TIME)
            {
                line++;
                fin>>startTime;
                fin>>str;
                if(!(strcmp(str, "end"))||!(strcmp(str, "restart")))
		    cout<<"Waiting for end or restart in: "<<startTime-currTime<<" seconds"<<endl;
                while(startTime>(currTime+CORRECTION-add))//Loops until the execution time for the next command is reached.
                {
                //Duel time determination: Uses time from getTime() when it has changed since the last time it retrieved it.
                //Otherwise it uses the number of clock cycles since the last time it retrieved a new time to calculate the current time.
                //EXCEPT if it has been more than 0.6 seconds since getTime() changed, in which case it assumes the user has paused audio
                //playback and waits until getTime() changes to continue light display execution.
                    if ((getTime()+CORRECTION2)!=prevTime&& getTime()!=-1)
                    {
                        currTime=(getTime()+CORRECTION2);
                        prevTime=currTime;
                    }
                    else if (prevTime>currTime-0.6)
                        currTime+=(clock()-initial_time)/CLOCKS_PER_SEC;
                    else if ((strcmp(str, "restart")&&strcmp(str, "end")))
                    {
                        cout<<"PAUSED"<<endl;
                        while(((getTime()+CORRECTION2)==prevTime|| getTime()==-1));//Loops until getTime changes.
                    }
		    else
                    {
                        currTime+=(clock()-initial_time)/CLOCKS_PER_SEC;
                    }

                    initial_time=clock();
                }
                if(!strcmp(str, "play"))//requires filename of song (start time must be zero)
                {                       //Plays specified mp3 file.
                    fin>>song;
                    if(music.joinable())
                        music.join();
                    music=thread(playsong, song);
                    while(getTime()<0||getTime()==initTime);//waits until song starts to move on.
                    initial_time=clock();//Number of clock cycles (since boot?). Used to calculate current time in-between getTime() updates.
                }
                else if(!strcmp(str, "on")||!strcmp(str, "fade_in"))//requires channel; fade_in included for legacy support.
                {                                                   //Sets specified channel to on state.
                    fin>>channel;
                    channel_setter(channel, 1, setchan, mirror);
                }
                else if(!strcmp(str, "off")||!strcmp(str, "fade_out"))//requires channel; fade_out included for legacy support.
                {                                                     //Sets specified channel to off state.
                    fin>>channel;
                    channel_setter(channel, 0, setchan, mirror);
                }
                else if(!strcmp(str, "restart"))//Marks the end of the display and triggers it to replay once audio ends.
                {
                    restart=true;
					fin>>str;
					if(!(strcmp(str, "night")))
					nightWait();
                    strcpy(str, "end");
                }
                else if(!strcmp(str, "add"))//Requires number of seconds to add.
                {                           //Essentially, adds specified time to all subsequent "startTime" values to cause light execution to shift.
                    fin>>temp_add;
                    add+=temp_add;
                }
                else if(!strcmp(str, "mirror"))//Requires mirror channel(slave) and channel to be mirrored(master) (in that order).
                {                              //Forces a channel to copy exactly one other channel.
                    fin>>channel;
                    fin>>mirror[channel-1];
                }
                else if(!strcmp(str, "unmirror"))//Requires slave channel.
                {                                //Specified channel no longer copies another channel.
                    fin>>unmirror;
                    mirror[unmirror-1]=0;
                }
                else if(!strcmp(str, "end"))//Specifies end of display. No commands will execute after this and this utility will wait until audio stops to continue.
                {                           //See loop condition.
                    cout<<"END OF DISPLAY"<<endl;
                }
                else
                {
                    cout<<"ERROR: UNKNOWN COMMAND AT LINE(COMMAND): "<<line<<endl;
                }
            }
            fin.close();
            cout<<"JOINING MUSIC THREAD... ";
            if(music.joinable())
                music.join();
            cout<<"DONE."<<endl;
        }
        else
        {
            cout<<"ERROR: FILE NOT FOUND!!"<<endl;
        }
    }
    return 0;
}

void playsong(char* filename)
{
    string command="mpg123 -v --no-gapless -b 1024 -C ";
    command.append(filename);
    system(command.c_str());
    return;
}

//LOOPS INFINITELY IF TWO WAY MIRRORING IS SPECIFIED.
void channel_setter(int channel, int level, int *setchan, int *mirror)
{
    int setP=1;
    static int lastset=0;
    int dig0;
    int dig1;
    int dig2;
    digitalWrite(24, 0);//disables decoder (assuming active high enable)
    setP=(channel-1)/8;
    setchan[channel-1]=level;
    if (setP!=lastset)
    {
        //decoder values
        dig0=setP%2;
        dig1=dig0%2;
        dig2=dig1%2;
        digitalWrite(23, dig0);//least significant bit
        digitalWrite(22, dig1);
        digitalWrite(21, dig2);//most significant bit
    }
    //sets channel pins to appropriate values
    for(int i=0; i<8; i++)
    {
        digitalWrite(i, setchan[((setP)*8)+i]);//Sets pins to states of all 8 channels belonging to the modified set.
    }
    digitalWrite(24, 1);//Enables decoder (assuming active high enable)
    lastset=setP;//Allows decoder related pins to not be reset if next channel called belongs to the same set.
    for(int i=0; i<MAX_CHANNELS; i++)
    {
        if(mirror[i]==channel)
            channel_setter((i+1), level, setchan, mirror);//Sets all mirrored channels to same level.
    }
    return;
}
//winter solctice 5pm - 7:34am
//summer "      " 8:47 - 5:52am
//                +3:47   -1:42

//corrected summer 7:47 - 4:52
//corrected diff   +2:47(167m)  -2:42(162m)
void nightWait()
{
    struct tm *tm_struct;
    time_t raw_time = time(NULL);
    tm_struct = localtime(&raw_time);
    int minute = tm_struct->tm_min;
    int hour = tm_struct->tm_hour;
    int day = tm_struct->tm_yday;
    int realDay = day-11;
    float baseNight_begin = 16.83;//evening
    float baseNight_end = 7.74;//morning
    int daylightSavings = tm_struct->tm_isdst;
    float realHour = hour + (minute/60.0);
    realHour-=(daylightSavings?1:0);
    if (realDay<0)
	realDay+=365;
    if(realDay>183)
        realDay=183-(realDay-183);
    bool night=false;
    cout<<"Waiting for nighttime in: "<<baseNight_begin+(realDay/60.0)-realHour<<" hours"<<endl;
    while(!night)
    {
        raw_time = time(NULL);
        tm_struct = localtime(&raw_time);
        minute = tm_struct->tm_min;
        hour = tm_struct->tm_hour;
        day = tm_struct->tm_yday;
        realDay = day-11;
        baseNight_begin = 16.83;//evening
        baseNight_end = 7.74;//morning
        daylightSavings = tm_struct->tm_isdst;
        realHour = hour + (minute/60.0);
        realHour-=(daylightSavings?1:0);
        if (realDay<0)
	    realDay+=365;
        if(realDay>183)
            realDay=183-(realDay-183);
        if(((realHour<=baseNight_end-(realDay/60.0) && realHour>=0)||(realHour>=baseNight_begin+(realDay/60.0) && realHour<=24)))
		night=true;
    }
    return;
}

double getTime()
{
    double seconds=-1;
    ifstream tin;
    tin.open("time.txt");
    if (tin.is_open())
    {
        tin>>seconds;
        tin.close();
    }
    return seconds;
}
//0, 1, 2, 3, 4, 5, 6, 7 for latch inputs
//21, 22, 23 for decoder inputs
//24 for decoder enable
//Channel numbering starts at 1, not 0.
