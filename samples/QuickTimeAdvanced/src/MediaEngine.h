#pragma once
#include <boost/shared_ptr.hpp>
#include <mfmediaengine.h>
#include <mfapi.h>
#include <chrono>
#include <vector>
#include <tuple>
#include <Wincodec.h>
#include <d3d11_1.h>
#include <d2d1_1.h>
#include <boost/thread.hpp>

#include <cinder/Url.h>
#include <cinder/Vector.h>
#include <cinder/gl/Texture.h>
#include <cinder/msw/CinderMsw.h>

struct MediaEngineNotifyCallback abstract
{
public:
    virtual void OnMediaEngineEvent(DWORD meEvent) = 0;
};

class MediaEngineNotify;

class MediaEngine 
{
public:
	MediaEngine(void);
	virtual ~MediaEngine(void);

	class TimeRange 
	{
	public:
		void addRange(double startTime,double endTime);
		void clear();
		bool containsTime(double time);
		double getEnd(int index);
		int32_t getLength();
		double getStart(int index);
	private:
		typedef std::tuple<double,double> RangeType;
		std::vector <RangeType> mRangesList;
	};

	struct Error
	{
		USHORT errorCode;
		HRESULT extendedErrorCode;
	};

	class MediaEngineException : public ci::Exception
	{
		std::string forWhat;
	public:
		MediaEngineException(std::string message):forWhat(message){
			
		}

		MediaEngineException():forWhat(""){
		}
	    
		virtual const char* what() const throw()
		{
			if(forWhat == "")
				return "MediaEngine Exception Happened";
			else 
				return forWhat.c_str();
		}
	};

	static MediaEngineException mGeneralMediaEngineException;
	/*
	//we'll need to implement this as a COM object?
	class BasicSrcList : IMFMediaEngineSrcElements
	{

	};
	*/

	

	struct Obj : public MediaEngineNotifyCallback {
		Obj(void);
		~Obj(void);

		std::shared_ptr<IDXGIOutput> mDXGIOutput;
		IMFMediaEngine* mMediaEngine;
		std::shared_ptr<IMFMediaEngineEx> mMediaEngineEx;
		std::shared_ptr<MediaEngineNotify> notifyObj;
		std::shared_ptr<IDXGIAdapter> spAdapter;
		std::shared_ptr<ID2D1Factory> d2dfactory;
		std::shared_ptr<IDXGISurface1> dxgiSurface;
		IMFDXGIDeviceManager* mDeviceManager;
		
		D3D11_MAPPED_SUBRESOURCE mMapped;

		std::shared_ptr<ID3D11Device1>                mDX11Device;
		std::shared_ptr<ID3D11DeviceContext1>         mDX11DeviceContext;
		bool mEOS;
		bool mStopTimer;
		bool mIsPlaying;
		

		IMFMediaEngine* _me(){ 
			if(!mMediaEngine) throw MediaEngineException("MediaEngine not initalized");
			return mMediaEngine;}

		std::shared_ptr<IMFMediaEngineEx> _mex(){ 
			if(!mMediaEngineEx) throw MediaEngineException("MediaEngine not initalized");
			return mMediaEngineEx;}

		void initalize();
		void createDX11Device();
		MF_MEDIA_ENGINE_CANPLAY canPlayType(std::string mimeType);
		bool getAutoPlay();
		TimeRange getBuffered();
		ci::Url getCurrentSource();
		double getCurrentTime();
		double getDefaultPlaybackRate();
		double getDuration();
		Error getError();
		bool getLoop();
		bool getMuted();
		ci::Vec2i getNativeVideoSize();
		MF_MEDIA_ENGINE_NETWORK getNetworkState();
		double getPlaybackRate();
		TimeRange getPlayed();
		MF_MEDIA_ENGINE_PRELOAD getPreload();
		MF_MEDIA_ENGINE_READY getReadyState();
		TimeRange getSeekable();
		double getStartTime();
		ci::Vec2i getVideoAspectRatio();
		double getVolume();
		bool hasAudio();
		bool hasVideo();
		bool isEnded();
		bool isPaused();
		bool isSeeking();

		void load();
		bool onVideoStreamTick(LONGLONG *pPts);
		void pause();
		void play();
		void setAutoPlay(bool autoPlay);
		void setCurrentTime(double seekTime);
		void setDefaultPlaybackRate(double rate);
		void setErrorCode(MF_MEDIA_ENGINE_ERR e);
		void setLoop(bool loop);
		void setMuted(bool muted);
		void setPlaybackRate(double rate);
		void setPreload(MF_MEDIA_ENGINE_PRELOAD preload);
		void setSource(ci::Url url);
		//void setSourceElements(IMFMediaEngineSrcElements elements);
		void setVolume(double volume);
		void shutdown();
		void startTimer();
		void stopTimer();
		DWORD RealVSyncTimer();
		
		//void transferVideoFrame(IUnknown *pDstSurf,const MFVideoNormalizedRect *pSrc,const RECT *pDst,const MFARGB *pBorderClr);
		
		ci::gl::Texture& getTexture(); //transferVideoFrame
		//std::shared_ptr<IWICBitmap> mWicBitmap;
		std::shared_ptr<ID3D11Texture2D> mD3D11Texture;
		std::shared_ptr<ID3D11Texture2D> mD3D11TextureRead;
		ci::Surface mSurf;
		ci::gl::Texture mTex;

		void OnMediaEngineEvent(DWORD meEvent);
		TimeRange toTimeRange(IMFMediaTimeRange* _timerange,bool release=true);
		MFARGB backColor;
		CRITICAL_SECTION mCritSec;

	};

private:
	std::shared_ptr<Obj>		mObj;
public:
	typedef std::shared_ptr<Obj> MediaEngine::*unspecified_bool_type;
	operator unspecified_bool_type() const { return ( mObj.get() == 0 ) ? 0 : &MediaEngine::mObj; }
	void reset() { mObj.reset(); }

	void initalize(){mObj->initalize();}
	MF_MEDIA_ENGINE_CANPLAY canPlayType(std::string mimeType){return mObj->canPlayType(mimeType);}
	bool getAutoPlay(){return mObj->getAutoPlay();};
	TimeRange getBuffered(){return mObj->getBuffered();}
	ci::Url getCurrentSource(){return mObj->getCurrentSource();}
	double getCurrentTime(){return mObj->getCurrentTime();}
	double getDefaultPlaybackRate(){return mObj->getDefaultPlaybackRate();}
	double getDuration(){return mObj->getDuration();}
	Error getError(){return mObj->getError();}
	bool getLoop(){return mObj->getLoop();}
	bool getMuted(){return mObj->getMuted();}
	ci::Vec2i getNativeVideoSize(){return mObj->getNativeVideoSize();}
	MF_MEDIA_ENGINE_NETWORK getNetworkState(){return mObj->getNetworkState();}
	double getPlaybackRate(){return mObj->getPlaybackRate();}
	TimeRange getPlayed(){return mObj->getPlayed();}
	MF_MEDIA_ENGINE_PRELOAD getPreload(){return mObj->getPreload();}
	MF_MEDIA_ENGINE_READY getReadyState(){return mObj->getReadyState();}
	TimeRange getSeekable(){return mObj->getSeekable();}
	double getStartTime(){return mObj->getStartTime();}
	ci::Vec2i getVideoAspectRatio(){return mObj->getVideoAspectRatio();}
	double getVolume(){return mObj->getVolume();}
	bool hasAudio(){return mObj->hasAudio();}
	bool hasVideo(){return mObj->hasVideo();}
	bool isEnded(){return mObj->isEnded();}
	bool isPaused(){return mObj->isPaused();}
	bool isSeeking(){return mObj->isSeeking();}

	void load(){mObj->load();}
	bool onVideoStreamTick(LONGLONG *pPts){mObj->onVideoStreamTick(pPts);}
	void pause(){mObj->pause();}
	void play(){mObj->play();}
	void setAutoPlay(bool autoPlay){mObj->setAutoPlay(autoPlay);}
	void setCurrentTime(double seekTime){mObj->setCurrentTime(seekTime);}
	void setDefaultPlaybackRate(double rate){mObj->setDefaultPlaybackRate(rate);}
	void setErrorCode(MF_MEDIA_ENGINE_ERR e){mObj->setErrorCode(e);}
	void setLoop(bool loop){mObj->setLoop(loop);}
	void setMuted(bool muted){mObj->setMuted(muted);}
	void setPlaybackRate(double rate){mObj->setPlaybackRate(rate);}
	void setPreload(MF_MEDIA_ENGINE_PRELOAD preload){mObj->setPreload(preload);}
	void setSource(ci::Url url){mObj->setSource(url);}
	//void setSourceElements(IMFMediaEngineSrcElements elements);
	void setVolume(double volume){mObj->setVolume(volume);}
	void shutdown(){mObj->shutdown();}

	//void transferVideoFrame(IUnknown *pDstSurf,const MFVideoNormalizedRect *pSrc,const RECT *pDst,const MFARGB *pBorderClr);

	ci::gl::Texture& getTexture(){return mObj->getTexture();} //transferVideoFrame

	ci::Surface& getSurface(){return mObj->mSurf;}
};

