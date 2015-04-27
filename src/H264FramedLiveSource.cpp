/*
 * Filename:  H264FramedLiveSource.cpp
 * Auther:  mlj
 * Create date: 2013/ 1/22
 */

#include "H264FramedLiveSource.hh"

char tmpbuf[300000];
unsigned int tailptr=0;

H264FramedLiveSource::H264FramedLiveSource( UsageEnvironment& env,  
	char const* fileName, 
	unsigned preferredFrameSize, 
	unsigned playTimePerFrame )
	: FramedSource(env)
{
	currentptr=stream_buf_ptr_tail;
	tailptr=0;
    // fp = fopen( fileName, "rb" ); 
}

H264FramedLiveSource* H264FramedLiveSource::createNew( UsageEnvironment& env,
	                                       char const* fileName, 
	                                       unsigned preferredFrameSize /*= 0*/, 
	                                       unsigned playTimePerFrame /*= 0*/ )
{ 
	H264FramedLiveSource* newSource = new H264FramedLiveSource(env, fileName, preferredFrameSize, playTimePerFrame);
 
	return newSource;
}

H264FramedLiveSource::~H264FramedLiveSource()
{
	//fclose(fp);
}


long filesize(FILE *stream)
{
	long curpos, length;
	curpos = ftell(stream);
	fseek(stream, 0L, SEEK_END);
	length = ftell(stream);
	fseek(stream, curpos, SEEK_SET);
	return length;
}


void H264FramedLiveSource::doGetNextFrame()
{

	while(currentptr==stream_buf_ptr_tail) usleep(1000);
	
	while(currentptr!=stream_buf_ptr_tail)
	{
		if(stream_buf_len[currentptr]+tailptr>300000) stream_buf_len[currentptr]=300000-tailptr;
		memcpy(tmpbuf+tailptr,(char *)stream_buf[currentptr],stream_buf_len[currentptr]);
		tailptr+=stream_buf_len[currentptr];
		currentptr=(currentptr+1)%stream_buf_size;	
	}
	

	if(tailptr>fMaxSize)
	{
		tailptr-=fMaxSize;
		fFrameSize=fMaxSize;
		memcpy(fTo,tmpbuf,fMaxSize);
		memcpy(tmpbuf,tmpbuf+fMaxSize,tailptr);
	}
	else
	{
		fFrameSize=tailptr;
		memcpy(fTo,tmpbuf,tailptr);
		tailptr=0;
	}

	//printf("%d\n",fFrameSize);
	
	/*if( filesize(fp) >  fMaxSize)
	fFrameSize = fread(fTo,1,fMaxSize,fp); 
	else
	{
	fFrameSize = fread(fTo,1,filesize(fp),fp);
	fseek(fp, 0, SEEK_SET);
	}
	fFrameSize = fMaxSize;*/
	nextTask() = envir().taskScheduler().scheduleDelayedTask( 0,
		(TaskFunc*)FramedSource::afterGetting, this);//表示延迟0秒后再执行 afterGetting 函数
	return;
}

