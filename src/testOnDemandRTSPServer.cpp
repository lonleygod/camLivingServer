#include <iostream>
using namespace std;
#include <string>
#include <assert.h>
#include "stdint.h"
//#include "y4m.h"


#include <cv.h>
#include <highgui.h>


#include "H264LiveVideoServerMediaSubssion.hh"
#include "H264FramedLiveSource.hh"
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"


//#pragma comment (lib, "Ws2_32.lib")  
//#pragma comment (lib, "MyLive555.lib")

UsageEnvironment* env;

extern "C"
{
#include "x264.h"
#include "x264_config.h"
};


//转换矩阵
#define MY(a,b,c) (( a*  0.2989  + b*  0.5866  + c*  0.1145))
#define MU(a,b,c) (( a*(-0.1688) + b*(-0.3312) + c*  0.5000 + 128))
#define MV(a,b,c) (( a*  0.5000  + b*(-0.4184) + c*(-0.0816) + 128))

//大小判断
#define DY(a,b,c) (MY(a,b,c) > 255 ? 255 : (MY(a,b,c) < 0 ? 0 : MY(a,b,c)))
#define DU(a,b,c) (MU(a,b,c) > 255 ? 255 : (MU(a,b,c) < 0 ? 0 : MU(a,b,c)))
#define DV(a,b,c) (MV(a,b,c) > 255 ? 255 : (MV(a,b,c) < 0 ? 0 : MV(a,b,c)))
#define CLIP(a) ((a) > 255 ? 255 : ((a) < 0 ? 0 : (a)))

#define image_buf_size 200
#define stream_buf_size 200


unsigned char *image_buf[image_buf_size];
int image_buf_ptr_head=0,image_buf_ptr_tail=0;
unsigned char *stream_buf[stream_buf_size];
int stream_buf_len[stream_buf_size];
int stream_buf_ptr_head=0,stream_buf_ptr_tail=0;
int image_width=640,image_height=480;


void *  OpenCV_Show(void *)
{
	//声明IplImage指针  
	IplImage* pFrame;// = cvCreateImage(cvSize(640,480),IPL_DEPTH_8U,3); 
	//获取摄像头  
	CvCapture* pCapture = cvCreateCameraCapture(-1);  

    cvSetCaptureProperty(pCapture,CV_CAP_PROP_FRAME_WIDTH,640);
    cvSetCaptureProperty(pCapture,CV_CAP_PROP_FRAME_HEIGHT,480);

	//显示视屏  
	while(1)  
	{  
		pFrame=cvQueryFrame( pCapture );  
		//cvResize(prepFrame,pFrame);
        
		if(!pFrame) continue;
		if ((pFrame->width!=image_width)||(pFrame->height!=image_height))
		{
			image_width=pFrame->width;
			image_height=pFrame->height;
		}
		if ((image_buf_ptr_tail+1)%image_buf_size!=image_buf_ptr_head)
		{
			image_buf[image_buf_ptr_tail]=(unsigned char *)malloc(image_width*image_height*3/2);
			unsigned char *Y = NULL;
			unsigned char *U = NULL;
			unsigned char *V = NULL;
			unsigned char *RGB=(unsigned char *)pFrame->imageData;
			Y = image_buf[image_buf_ptr_tail];
			U = Y + image_width*image_height;
			V = U + ((image_width*image_height)>>2);
			for(int y=0; y < image_height; y++)
				for(int x=0; x < image_width; x++)
				{
					int i = y*pFrame->widthStep+x*3;
					int j = y*image_width+x;
					Y[j] = (unsigned char)(DY(RGB[i+2], RGB[i+1], RGB[i]));
					if(x%2 == 0 && y%2 == 0)
					{
						j = (image_width>>1) * (y>>1) + (x>>1);
						U[j] = (unsigned char)DU(RGB[i+2], RGB[i+1], RGB[i]);
						V[j] = (unsigned char)DV(RGB[i+2], RGB[i+1], RGB[i]);
					}
				}
				image_buf_ptr_tail=(image_buf_ptr_tail+1)%image_buf_size;
		}
        
		cvShowImage("Sender",pFrame);  
		char c=cvWaitKey(30);  
		if(c==27)break;  
	}  
	cvReleaseCapture(&pCapture);  
	cvDestroyWindow("video");
    return NULL;
}

void * H264_Encoder(void * )
{
     x264_t* pX264Handle   = NULL;  
     x264_param_t* pX264Param = new x264_param_t;  
     assert(pX264Param);  
     while(image_buf_ptr_head==image_buf_ptr_tail) usleep(1000);
	 //* 配置参数  
     //* 使用默认参数，在这里因为我的是实时网络传输，所以我使用了zerolatency的选项，使用这个选项之后就不会有delayed_frames，如果你使用的不是这样的话，还需要在编码完成之后得到缓存的编码帧  
     x264_param_default_preset(pX264Param, "veryfast", "zerolatency");  
     //* cpuFlags  
     pX264Param->i_threads  = X264_SYNC_LOOKAHEAD_AUTO;//* 取空缓冲区继续使用不死锁的保证.  
     //* 视频选项  
     pX264Param->i_width   = image_width; //* 要编码的图像宽度.  
     pX264Param->i_height  = image_height; //* 要编码的图像高度  
     pX264Param->i_frame_total = 0; //* 编码总帧数.不知道用0.  
     pX264Param->i_keyint_max = 10;   
     //* 流参数  
     pX264Param->i_bframe  = 5;  
     pX264Param->b_open_gop  = 0;  
     pX264Param->i_bframe_pyramid = 0;  
     pX264Param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;  
     //* Log参数，不需要打印编码信息时直接注释掉就行  
     //pX264Param->i_log_level  = X264_LOG_DEBUG;  
     //* 速率控制参数
	 pX264Param->rc.i_rc_method = X264_RC_ABR;
     pX264Param->rc.i_bitrate = 512;//* 码率(比特率,单位Kbps)

	 //* muxing parameters  
     pX264Param->i_fps_den  = 1; //* 帧率分母  
     pX264Param->i_fps_num  = 30;//* 帧率分子  
     pX264Param->i_timebase_den = pX264Param->i_fps_num;  
     pX264Param->i_timebase_num = pX264Param->i_fps_den;  
     //* 设置Profile.使用Baseline profile  
     x264_param_apply_profile(pX264Param, x264_profile_names[0]);  
  
     //* 编码需要的辅助变量  
     int iNal = 0;  
     x264_nal_t * pNals = NULL;  
     x264_picture_t* pPicIn = new x264_picture_t;  
     x264_picture_t* pPicOut = new x264_picture_t;  
     x264_picture_init(pPicOut);  
     x264_picture_alloc(pPicIn, X264_CSP_I420, pX264Param->i_width, pX264Param->i_height);  
     pPicIn->img.i_csp = X264_CSP_I420;  
     pPicIn->img.i_plane = 3;  
  
     pX264Handle = x264_encoder_open(pX264Param);  
     assert(pX264Handle);  
  
     int framecount=0;   
	 while(1)
	 {
		usleep(1000);
		while(image_buf_ptr_head!=image_buf_ptr_tail)  
		{  
			unsigned char *Y,*U,*V;
			Y=image_buf[image_buf_ptr_head];
			U=Y+image_width*image_height;
			V=U+((image_width*image_height)>>2);
			memcpy(pPicIn->img.plane[0],Y,image_width*image_height);
			memcpy(pPicIn->img.plane[1],U,(image_width*image_height)>>2);
			memcpy(pPicIn->img.plane[2],V,(image_width*image_height)>>2);
			pPicIn->i_pts = framecount;   

			//编码  
			int frame_size = x264_encoder_encode(pX264Handle,&pNals,&iNal,pPicIn,pPicOut);  
			if(frame_size >0)//&&((stream_buf_ptr_tail+1)%stream_buf_size!=stream_buf_ptr_head))  
			{  
				int length=0;
				for(int i=0;i<iNal;i++) length+=pNals[i].i_payload;
				if(stream_buf[stream_buf_ptr_tail]!=NULL) free(stream_buf[stream_buf_ptr_tail]);
				stream_buf[stream_buf_ptr_tail]=(unsigned char *)malloc(length);
				//printf("%d %d\n",stream_buf_ptr_tail,length);
				unsigned char *ptr=stream_buf[stream_buf_ptr_tail];
				for (int i = 0; i < iNal; ++i)  
				{ 
					memcpy(ptr,pNals[i].p_payload,pNals[i].i_payload);
					ptr=ptr+pNals[i].i_payload;
				}  
				stream_buf_len[stream_buf_ptr_tail]=length;
				stream_buf_ptr_tail=(stream_buf_ptr_tail+1)%stream_buf_size;
			}  
			free(image_buf[image_buf_ptr_head]);
			image_buf[image_buf_ptr_head]=NULL;
			image_buf_ptr_head=(image_buf_ptr_head+1)%image_buf_size;
		}
	 }
  
     //* 清除图像区域  
     x264_picture_clean(pPicIn);  
     //* 关闭编码器句柄  
     x264_encoder_close(pX264Handle);  
     pX264Handle = NULL;  
     delete pPicIn ;  
     pPicIn = NULL;  
     delete pPicOut;  
     pPicOut = NULL;  
     delete pX264Param;  
     pX264Param = NULL;
}


// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = True;

// To stream *only* MPEG-1 or 2 video "I" frames
// (e.g., to reduce network bandwidth),
// change the following "False" to "True":
Boolean iFramesOnly = False;

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName, char const* inputFileName); // fwd

static char newMatroskaDemuxWatchVariable;
static MatroskaFileServerDemux* demux;
static void onMatroskaDemuxCreation(MatroskaFileServerDemux* newDemux, void* /*clientData*/) 
{
  demux = newDemux;
  newMatroskaDemuxWatchVariable = 1;
}

void * live555(void *) 
{
    // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  UserAuthenticationDatabase* authDB = NULL;
#ifdef ACCESS_CONTROL
  // To implement client access control to the RTSP server, do the following:
  authDB = new UserAuthenticationDatabase;
  authDB->addUserRecord("username1", "password1"); // replace these with real strings
  // Repeat the above with each <username>, <password> that you wish to allow
  // access to the server.
#endif

  // Create the RTSP server:
  RTSPServer* rtspServer = RTSPServer::createNew(*env, 554, authDB);
  if (rtspServer == NULL) {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
  }

  char const* descriptionString
    = "Session streamed by \"live\"";


  // A H.264 video elementary stream:
  {
    char const* streamName = "live";
    char const* inputFileName = "test2.264";
    ServerMediaSession* sms
      = ServerMediaSession::createNew(*env, streamName, streamName,
				      descriptionString);
    sms->addSubsession(H264LiveVideoServerMediaSubssion
		       ::createNew(*env, inputFileName, reuseFirstSource));//修改为自己实现的servermedia  H264LiveVideoServerMediaSubssion
    rtspServer->addServerMediaSession(sms);

    announceStream(rtspServer, sms, streamName, inputFileName);
  }

  env->taskScheduler().doEventLoop(); // does not return

  return 0; // only to prevent compiler warning
}

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,char const* streamName, char const* inputFileName) 
{
		char* url = rtspServer->rtspURL(sms);
		UsageEnvironment& env = rtspServer->envir();
		env << "\n\"" << streamName << "\" stream, from the file \""
			<< inputFileName << "\"\n";
		env << "Play this stream using the URL \"" << url << "\"\n";
		delete[] url;
}




int main()
{

	for(int i=0;i<stream_buf_size;i++) stream_buf[i]=NULL;
    
    pthread_t id1,id2;
    pthread_create(&id2,NULL,&H264_Encoder,NULL);
    pthread_create(&id1,NULL,&live555,NULL);


    OpenCV_Show(NULL);
    return 0;
  
}
