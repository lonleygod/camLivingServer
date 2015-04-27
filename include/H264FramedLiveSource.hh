/*
 * Filename: H264FramedLiveSource.hh
 * Auther: chenbin
 * Create date: 2013/ 1/22
 */

#ifndef _H264FRAMEDLIVESOURCE_HH
#define _H264FRAMEDLIVESOURCE_HH

#include <FramedSource.hh>


#define FRAME_PER_SEC 20
#define stream_buf_size 200
extern int stream_buf_ptr_tail;
extern unsigned char *stream_buf[stream_buf_size];
extern int stream_buf_len[stream_buf_size];



class H264FramedLiveSource : public FramedSource
{
public:
	static H264FramedLiveSource* createNew(UsageEnvironment& env,
		char const* fileName,
		unsigned preferredFrameSize = 0,
		unsigned playTimePerFrame = 0); 

protected:
	H264FramedLiveSource(UsageEnvironment& env,
		char const* fileName, 
		unsigned preferredFrameSize,
		unsigned playTimePerFrame);
	// called only by createNew()
	~H264FramedLiveSource();

private:
	// redefined virtual functions:
	virtual void doGetNextFrame();
	int TransportData( unsigned char* to, unsigned maxSize );

protected:
    FILE *fp;
	int currentptr;
};

#endif
