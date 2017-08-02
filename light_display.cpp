using namespace std;

#include <iostream>
#include <ctime>
#include <time.h>
#include <wiringPi.h>
#include <fstream>
#include <string.h>
#include <thread>
#include <stdlib.h>
#include "light_display.h"

void light_display::light_display(int channels)
{
	currentChannels=channels;
	initializeLog();
	initializeChannels();
	for(int i=0; i<MAX_CHANNELS; i++)
	{
		mirror[i]=-1;
		setchan[i]=0;
	}
	//return;
}

string light_display::timeStamp()
{
	time_t _tm =time(NULL );
	struct tm * curtime = localtime ( &_tm );
	return asctime(curtime);
}

void light_display::initializeLog()
{
	log.open(LOG_NAME);
	if(log.is_open())
		logMessage("LOG INITIALIZED.");
	else
		cout<<"ERROR: Log failed to initialize. Ignored."<<endl;
	return;
}

void light_display::logMessage(string message)
{
	if(log.is_open())
		log<<timeStamp<<"	"<<message<<endl;
	else
		cout<<"ERROR: Log file failed to open with Message: "<<message<<" Ignored."
	return;
}

void light_display::reset()
{
	logMessage("Initial time/channel values reset.");
	for(int i=0; i<MAX_CHANNELS; i++)
	{
		mirror[i]=-1;
		setchan[i]=0;
	}
	initTime=getTime();
	initial_time=0;
	line=0;
	add=0;
	currTime=0.0;
	strcpy(str, "start");
	prevTime=getTime();
	return;
}

void light_display::executeCommand()
{
	line++;
	fin>>startTime;
	fin>>str;
	if(!(strcmp(str, "end"))||!(strcmp(str, "restart"))||!(strcmp(str, "continue")))
	{
		cout<<"Waiting for end, restart, or continue in: "<<startTime-currTime<<" seconds"<<endl;
		logMessage("Waiting for end, restart, or continue in: "+to_string(startTime-currTime)+" seconds");
	}
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
	logMessage("Executing "+str+" At line: "+to_string(line));
	if(!strcmp(str, "play"))//requires filename of song (start time must be zero)
	{                       //Plays specified mp3 file.
		fin>>song;
		if(music.joinable())
			music.join();
		music=thread(playsong, song);
		logMessage("Song has been started on seperate thread.");
		while(getTime()<0||getTime()==initTime);//waits until song starts to move on.
		initial_time=clock();//Number of clock cycles (since boot?). Used to calculate current time in-between getTime() updates.
	}
	else if(!strcmp(str, "on")||!strcmp(str, "fade_in"))//requires channel; fade_in included for legacy support.
	{                                                   //Sets specified channel to on state.
		fin>>channel;
		channel_setter(channel, 1);
	}
	else if(!strcmp(str, "off")||!strcmp(str, "fade_out"))//requires channel; fade_out included for legacy support.
	{                                                     //Sets specified channel to off state.
		fin>>channel;
		channel_setter(channel, 0);
	}
	else if(!strcmp(str, "restart"))//Marks the end of the display and triggers it to replay once audio ends.
	{
		restart=true;
		fin>>str;
		if(!(strcmp(str, "night")))
			nightWait();
		strcpy(str, "restart");
		logMessage("Restarting show now.");
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
		logMessage("Channel "+to_string(channel)+" is mirroring channel "+to_string(mirror[channel-1]));
	}
	else if(!strcmp(str, "unmirror"))//Requires slave channel.
	{                                //Specified channel no longer copies another channel.
		fin>>unmirror;
		mirror[unmirror-1]=0;
		logMessage("Channel "+to_string(unmirror)+" is no longer mirroring a channel.");
	}
	else if(!strcmp(str, "end"))//Specifies end of display. No commands will execute after this and this utility will wait until audio stops to continue.
	{                           //See loop condition.
		cout<<"END OF DISPLAY"<<endl;
		logMessage("END OF DISPLAY.");
	}
	else if(!strcmp(str, "continue"))//Specifies end of show. No commands will execute after this and this utility will wait until audio stops to continue.
	{                           //See loop condition.
		cout<<"END OF SHOW"<<endl<<endl;
		logMessage("END OF SHOW.");
	}
	else
	{
		cout<<"ERROR: UNKNOWN COMMAND AT LINE(COMMAND): "<<line<<endl;
		logMessage("Command: "+str+" Not Found!");
	}
	return;
}


void light_display::runDisplay(string show="")
{
	//initializeLog();
	//initializeChannels();
    while(strcmp(str, "end"))
    {
		logMessage("Beginning Show.");
		reset();
        if(restart==false&&show=="")//This block of code must be skipped if a "restart" command was used in the last show file execution.
        {
            cout<<"Type in the name of the show file you would like to play: ";
            cin>>filename;
            //If no file extension is specified, ".show" is automatically appended to the end of the given filename.
            found=filename.find('.');
            if (found==string::npos)
                filename.append(".show");
        }
        else if (show!="")
		{
			filename=show;
            //If no file extension is specified, ".show" is automatically appended to the end of the given filename.
            found=filename.find('.');
            if (found==string::npos)
                filename.append(".show");
			show="";
		}
        restart=false;
        fin.open(filename.c_str());
        if (fin.is_open())//read show file
        {
			logMessage("Starting Show: "+filename);
            while(strcmp(str, "end")&&strcmp(str, "restart")&&strcmp(str, "continue"))//"end" ends the loop (ALL COMMANDS ARE PRECEEDED BY A START TIME)
            {
				executeCommand();
            }
            fin.close();
            cout<<"JOINING MUSIC THREAD... ";
			logMessage("Waiting for music thread to join...");
            if(music.joinable())
                music.join();
			logMessage("Music thread joined.");
            cout<<"DONE."<<endl;
        }
        else
        {
            cout<<"ERROR: FILE NOT FOUND!!"<<endl;
			logMessage("Show file "+filename+" not found!");
        }
    }
	reset();
    return;
}

void light_display::initializeChannels()
{
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
	logMessage("GPIO pins initialized.");
	return;
}
	
void light_display::playsong(char* filename)
{
    string command="mpg123 -v --no-gapless -b 1024 -C ";
    command.append(filename);
    system(command.c_str());
    return;
}

//LOOPS INFINITELY IF TWO WAY MIRRORING IS SPECIFIED.
void light_display::channel_setter(int channel, int level)
{
    int setP;
    static int lastset=-1;
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
		logMessage("Decoder set to: "+to_string(dig2)+to_string(dig1)+to_string(dig0));
    }
    //sets channel pins to appropriate values
    for(int i=0; i<8; i++)
    {
        digitalWrite(i, setchan[((setP)*8)+i]);//Sets pins to states of all 8 channels belonging to the modified set.
		logMessage("GPIO pin "+to_string(i)+" set to "+to_string(setchan[((setP)*8)+i]));
    }
    digitalWrite(24, 1);//Enables decoder (assuming active high enable)
    lastset=setP;//Allows decoder related pins to not be reset if next channel called belongs to the same set.
    for(int i=0; i<currentChannels; i++)
    {
        if(mirror[i]==channel)
            channel_setter((i+1), level);//Sets all mirrored channels to same level.
    }
    return;
}
//winter solctice 5pm - 7:34am
//summer "      " 8:47 - 5:52am
//                +3:47   -1:42

//corrected summer 7:47 - 4:52
//corrected diff   +2:47(167m)  -2:42(162m)
void light_display::nightWait()
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
	string message = "Waiting for nighttime in: "+to_string(baseNight_begin+(realDay/60.0)-realHour)+" hours";
	logMessage(message);
    cout<<message<<endl;
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
	logMessage("Exiting nightWait (night detected)");
	reset();
    return;
}

double light_display::getTime()
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