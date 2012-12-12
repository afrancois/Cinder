
#include "MediaEngine.h"
#include "comutil.h"
#include "cinder/app/AppBasic.h"
#include <ppl.h>

using namespace ci;
using namespace std;
using namespace cinder::msw;
using namespace Concurrency;

MediaEngine::MediaEngineException MediaEngine::mGeneralMediaEngineException;

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		// Set a breakpoint on this line to catch DirectX API errors
		throw MediaEngine::mGeneralMediaEngineException;
	}
}

inline void ThrowIfFailed(HRESULT hr,std::string fail)
{
	if (FAILED(hr))
	{
		// Set a breakpoint on this line to catch DirectX API errors
		MediaEngine::MediaEngineException ex(fail);
		throw ex;
	}
}

inline void ThrowIfFailed(HRESULT hr,std::string fail,std::string success,bool debugOnly=true)
{
	if (FAILED(hr))
	{

		// Set a breakpoint on this line to catch DirectX API errors
		MediaEngine::MediaEngineException ex(fail);
		throw ex;
	} else {
#ifndef _DEBUG
		if(!debugOnly){
#endif
			OutputDebugStringA((success+"\n").c_str());
			cout<<success<<endl;
#ifndef _DEBUG
		}
#endif
	}
}


/*
**  MediaEngineNotify
**  Com Object that receives events from the MediaEngine/Movie
*/
class MediaEngineNotify : public IMFMediaEngineNotify
{
	long m_cRef;
	MediaEngineNotifyCallback* m_pCB;

public:
	MediaEngineNotify() : m_cRef(1), m_pCB(nullptr)
	{
	}
#pragma region Required COM Methods
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
		if(__uuidof(IMFMediaEngineNotify) == riid)
		{
			*ppv = static_cast<IMFMediaEngineNotify*>(this);
		}
		else
		{
			*ppv = nullptr;
			return E_NOINTERFACE;
		}

		AddRef();

		return S_OK;
	}      

	STDMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&m_cRef);
	}

	STDMETHODIMP_(ULONG) Release()
	{
		LONG cRef = InterlockedDecrement(&m_cRef);
		if (cRef == 0)
		{
			delete this;
		}
		return cRef;
	}
#pragma endregion

	// EventNotify is called when the Media Engine sends an event.
	STDMETHODIMP EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD param2)
	{
		if (meEvent == MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE)
		{
			SetEvent(reinterpret_cast<HANDLE>(param1));			
		}
		else
		{
			//pass the event on to the callback;
			m_pCB->OnMediaEngineEvent(meEvent);			
		}

		return S_OK;
	}

	void MediaEngineNotifyCallback(MediaEngineNotifyCallback* pCB)
	{
		m_pCB = pCB;
	}
};

MediaEngine::MediaEngine(void)
{
	mObj=shared_ptr<Obj>(new Obj());
}


MediaEngine::~MediaEngine(void)
{
}

MediaEngine::Obj::Obj(void) : mEOS(false)
{
	backColor.rgbAlpha=0;
	backColor.rgbRed=0;
	backColor.rgbGreen=0;
	backColor.rgbBlue=0;
	InitializeCriticalSectionEx(&mCritSec, 0, 0);
	mD3D11Texture.reset();
}

MediaEngine::Obj::~Obj(void){
	//destroy the mediaEngine
	stopTimer();
	mMediaEngine->Shutdown();
}

void MediaEngine::Obj::initalize()
{
	//initalize COM
	CoInitializeEx(NULL,COINIT_MULTITHREADED);
	
	DXGI_FORMAT m_d3dFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
	IMFMediaEngineClassFactory* _factory;
	IMFAttributes* _attributes=NULL;
	IMFMediaEngine* _mediaEngine=NULL;
	IMFMediaEngineEx* _mediaEngineEx=NULL;

	shared_ptr<IMFMediaEngineClassFactory> factoryObj;
	shared_ptr<IMFAttributes> attributesObj;    	    
	
	createDX11Device();
	UINT resetToken;
	ThrowIfFailed(
		MFCreateDXGIDeviceManager(&resetToken, &mDeviceManager)
		);

	ThrowIfFailed(
		mDeviceManager->ResetDevice(mDX11Device.get(), resetToken)
		);

	//Initalize the MediaFramework
	
	ThrowIfFailed(MFStartup(MF_VERSION));

	//setup notifications to the MediaEngine::Obj
	notifyObj = shared_ptr<MediaEngineNotify>(new MediaEngineNotify());
	if (notifyObj == nullptr)
	{
		ThrowIfFailed(E_OUTOFMEMORY);    
	}
	notifyObj->MediaEngineNotifyCallback(this);

	//Create the factory.
	ThrowIfFailed( CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_factory)),"failed to create factory" );
	factoryObj = makeComShared(_factory);



	// Set configuration attribiutes.
	ThrowIfFailed( MFCreateAttributes(&_attributes, 1) );
	attributesObj = makeComShared(_attributes);
	ThrowIfFailed( _attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, (IUnknown*) notifyObj.get()) );
	ThrowIfFailed(
		_attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, m_d3dFormat)
		);
	ThrowIfFailed(
		_attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, mDeviceManager)
		);

	// Create the Media Engine.
	//mMediaEngine = NULL;
	const DWORD flags = MF_MEDIA_ENGINE_WAITFORSTABLE_STATE;
	ThrowIfFailed( _factory->CreateInstance(flags, _attributes, &mMediaEngine) );
	
	ThrowIfFailed( mMediaEngine->QueryInterface(__uuidof(IMFMediaEngine), (void**) &_mediaEngineEx) );
	mMediaEngineEx = makeComShared(_mediaEngineEx);
	
}

//+-------------------------------------------------------------------------
//
//  Function:   CreateDX11Device()
//
//  Synopsis:   creates a default device
//
//--------------------------------------------------------------------------
void MediaEngine::Obj::createDX11Device()
{
	bool m_fUseDX = true;
    static const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,  
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    D3D_FEATURE_LEVEL FeatureLevel;
    HRESULT hr = S_OK;
	ID3D11Device* _device;
	ID3D11DeviceContext* _deviceContext;
	ID3D11Device1* _device1;
	ID3D11DeviceContext1* _deviceContext1;
    if(m_fUseDX)
    {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
			D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
            levels,
            ARRAYSIZE(levels),
            D3D11_SDK_VERSION,
            &_device,
            &FeatureLevel,
            &_deviceContext
            );
    }

	_device->QueryInterface (__uuidof (ID3D11Device1), (void **) &_device1);
	_deviceContext->QueryInterface (__uuidof (ID3D11DeviceContext1), (void **) &_deviceContext1);

	_device->Release();
	_deviceContext->Release();



    //Failed to create DX11 Device (using VM?), create device using WARP
    if(FAILED(hr))
    {
        m_fUseDX = FALSE;
    }

    if(!m_fUseDX)
    {
        ThrowIfFailed(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels,
            ARRAYSIZE(levels),
            D3D11_SDK_VERSION,
            &_device,
            &FeatureLevel,
			&_deviceContext
            ));
    }
	mDX11Device = makeComShared(_device1);
	mDX11DeviceContext = makeComShared(_deviceContext1);
    if(m_fUseDX)
    {
        ID3D10Multithread* spMultithread;
        ThrowIfFailed(
            mDX11Device.get()->QueryInterface(IID_PPV_ARGS(&spMultithread))
            );
        spMultithread->SetMultithreadProtected(TRUE);
		//spMultithread->Release();
    }

    return;
}

void MediaEngine::Obj::OnMediaEngineEvent(DWORD meEvent)
{
	switch (meEvent)
	{
	case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
		{				
			mEOS = FALSE;
		}
		break;
	case MF_MEDIA_ENGINE_EVENT_CANPLAY:			
		{        
			// Start the Playback
			play();				
		}
		break;        
	case MF_MEDIA_ENGINE_EVENT_PLAY:	
		mIsPlaying = TRUE;
		break;				
	case MF_MEDIA_ENGINE_EVENT_PAUSE:        
		mIsPlaying = FALSE;
		break;
	case MF_MEDIA_ENGINE_EVENT_ENDED:

		if(mMediaEngine->HasVideo())
		{
			stopTimer();
		}
		mEOS = TRUE;
		break;
	case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE:        
		break;
	case MF_MEDIA_ENGINE_EVENT_ERROR:        
		break;    
	}

	return;
}

MF_MEDIA_ENGINE_CANPLAY MediaEngine::Obj::canPlayType(string mimeType)
{
	_bstr_t convertString(mimeType.c_str());
	BSTR mimeTypeBSTR = convertString;
	MF_MEDIA_ENGINE_CANPLAY canplay;
	ThrowIfFailed(
		_me()->CanPlayType(mimeTypeBSTR,&canplay),
		"CanPlayType failed.");
	return canplay;
}

bool MediaEngine::Obj::getAutoPlay()
{
	return mMediaEngine->GetAutoPlay();
}

MediaEngine::TimeRange MediaEngine::Obj::getBuffered()
{
	IMFMediaTimeRange* _timerange;
	_me()->GetBuffered(&_timerange);
	return toTimeRange(_timerange);
}

ci::Url MediaEngine::Obj::getCurrentSource()
{
	BSTR url;
	_me()->GetCurrentSource(&url);
	std::string urlString((char*)url);
	return ci::Url(urlString);
}

double MediaEngine::Obj::getCurrentTime()
{
	return _me()->GetCurrentTime();
}

double MediaEngine::Obj::getDefaultPlaybackRate()
{
	return _me()->GetDefaultPlaybackRate();
}

double MediaEngine::Obj::getDuration()
{
	return _me()->GetDuration();
}

MediaEngine::Error MediaEngine::Obj::getError()
{
	Error err;
	IMFMediaError* mediaError;
	_me()->GetError(&mediaError);
	err.errorCode = mediaError->GetErrorCode();
	err.extendedErrorCode = mediaError->GetExtendedErrorCode();
	mediaError->Release();
	return err;
}

bool MediaEngine::Obj::getLoop()
{
	return _me()->GetLoop();
}

bool MediaEngine::Obj::getMuted()
{
	return _me()->GetMuted();
}

ci::Vec2i MediaEngine::Obj::getNativeVideoSize()
{
	DWORD cx,cy;
	_me()->GetNativeVideoSize(&cx,&cy);
	return Vec2i(cx,cy);
}

MF_MEDIA_ENGINE_NETWORK MediaEngine::Obj::getNetworkState()
{
	return (MF_MEDIA_ENGINE_NETWORK)_me()->GetNetworkState();
}

double MediaEngine::Obj::getPlaybackRate()
{
	return _me()->GetPlaybackRate();
}

MediaEngine::TimeRange MediaEngine::Obj::getPlayed()
{
	IMFMediaTimeRange* _timerange;
	_me()->GetPlayed(&_timerange);
	return toTimeRange(_timerange);
}

MF_MEDIA_ENGINE_PRELOAD MediaEngine::Obj::getPreload()
{
	return (MF_MEDIA_ENGINE_PRELOAD)_me()->GetPreload();
}

MF_MEDIA_ENGINE_READY MediaEngine::Obj::getReadyState()
{
	return (MF_MEDIA_ENGINE_READY)_me()->GetReadyState();
}

MediaEngine::TimeRange MediaEngine::Obj::getSeekable()
{
	IMFMediaTimeRange* _timerange;
	_me()->GetSeekable(&_timerange);
	return toTimeRange(_timerange);
}

double MediaEngine::Obj::getStartTime()
{
	return _me()->GetStartTime();
}

ci::Vec2i MediaEngine::Obj::getVideoAspectRatio()
{
	DWORD cx,cy;
	_me()->GetVideoAspectRatio(&cx,&cy);
	return Vec2i(cx,cy);
}

double MediaEngine::Obj::getVolume()
{
	return _me()->GetVolume();
}

bool MediaEngine::Obj::hasAudio()
{
	return _me()->HasAudio();
}

bool MediaEngine::Obj::hasVideo()
{
	return _me()->HasVideo();
}

bool MediaEngine::Obj::isEnded()
{
	return _me()->IsEnded();
}

bool MediaEngine::Obj::isPaused()
{
	return _me()->IsPaused();
}

bool MediaEngine::Obj::isSeeking()
{
	return _me()->IsSeeking();
}

void MediaEngine::Obj::load(){
	ThrowIfFailed( _me()->Load(), "Failed to Load Source(s)","MediaEngine Loaded Sources");
}

bool MediaEngine::Obj::onVideoStreamTick(LONGLONG *pPts)
{
	return _me()->OnVideoStreamTick(pPts);
}

void MediaEngine::Obj::pause()
{
	_me()->Pause();
}

void MediaEngine::Obj::play()
{
	 if (mMediaEngine)
    {
        if(_me()->HasVideo() && mStopTimer)
        {
            // Start the Timer thread
            startTimer();
        }

        if(mEOS)
        {
            setCurrentTime(0);   
            mIsPlaying = TRUE;            
        }
        else
        {
            ThrowIfFailed(
                _me()->Play()
                );
        }  

        mEOS = FALSE;
    }
    return;
}

void MediaEngine::Obj::setAutoPlay(bool autoPlay)
{
	_me()->SetAutoPlay(autoPlay);
}

void MediaEngine::Obj::setCurrentTime(double seekTime)
{
	_me()->SetCurrentTime(seekTime);
}

void MediaEngine::Obj::setDefaultPlaybackRate(double rate)
{
	_me()->SetDefaultPlaybackRate(rate);
}

void MediaEngine::Obj::setErrorCode(MF_MEDIA_ENGINE_ERR e)
{
	_me()->SetErrorCode(e);
}

void MediaEngine::Obj::setLoop(bool loop)
{
	_me()->SetLoop(loop);
}

void MediaEngine::Obj::setMuted(bool muted)
{
	_me()->SetMuted(muted);
}

void MediaEngine::Obj::setPlaybackRate(double rate)
{
	_me()->SetPlaybackRate(rate);
}

void MediaEngine::Obj::setPreload(MF_MEDIA_ENGINE_PRELOAD preload)
{
	_me()->SetPreload(preload);
}

void MediaEngine::Obj::setSource(ci::Url url)
{
	_bstr_t urlBSTR = _bstr_t(url.c_str());
	
	if(mMediaEngine){
		ThrowIfFailed( mMediaEngine->SetSource(urlBSTR.copy()), "Couldn't set URL");
		mMediaEngine->Load();
	}

}

void MediaEngine::Obj::setVolume(double volume)
{
	_me()->SetVolume(volume);
}

void MediaEngine::Obj::shutdown()
{
	_me()->Shutdown();
}

void MediaEngine::Obj::startTimer()
{

	IDXGIFactory1* _spFactory;
	ThrowIfFailed(
		CreateDXGIFactory1(IID_PPV_ARGS(&_spFactory))
		);
	shared_ptr<IDXGIFactory1> spFactory = makeComShared(_spFactory);


	IDXGIAdapter* _spAdapter;
	ThrowIfFailed(
		spFactory->EnumAdapters(0, &_spAdapter)
		);
	spAdapter = makeComShared(_spAdapter);


	IDXGIOutput* _spOutput;
	ThrowIfFailed(
		spAdapter->EnumOutputs(0, &_spOutput)
		);
	mDXGIOutput = makeComShared(_spOutput);
	mStopTimer = FALSE;

	auto vidPlayer = this;
	boost::thread([=](){
		vidPlayer->RealVSyncTimer();
	});

	return;
}

void MediaEngine::Obj::stopTimer()
{
	mStopTimer = TRUE;    
	mIsPlaying = FALSE;

	return;
}

DWORD MediaEngine::Obj::RealVSyncTimer()
{
	for ( ;; )
	{
		if (mStopTimer)
		{
			break;
		}

		if (SUCCEEDED(mDXGIOutput->WaitForVBlank()))
		{
			EnterCriticalSection(&mCritSec);

			if (mMediaEngine != nullptr)
			{
				LONGLONG pts;
				if (onVideoStreamTick(&pts) == S_OK)
				{
					Vec2i size = getNativeVideoSize();
					
					//Create a CPU readable texture 
					//and a CPU non-readble texture.
					//We need the non-readable texture to
					//get the frame from the movie
					//and the readable one to get the frame
					//to cinder. (Is there a better way?)
					ID3D11Texture2D* _tex;
					ID3D11Texture2D* _tex2;
					D3D11_TEXTURE2D_DESC texDesc;
					if(!mD3D11Texture)
					{
						
						memset( &texDesc, 0,sizeof(texDesc) );
						texDesc.Width = size.x;
						texDesc.Height = size.y;
						texDesc.MipLevels = 0;
						texDesc.ArraySize = 1;
						texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
						texDesc.SampleDesc.Count = 1;
						texDesc.SampleDesc.Quality =  0;
						texDesc.Usage = D3D11_USAGE_STAGING;
						texDesc.BindFlags = 0;
						texDesc.CPUAccessFlags =  D3D11_CPU_ACCESS_READ;
						texDesc.MiscFlags = 0;
					

						HRESULT hr = mDX11Device->CreateTexture2D(&texDesc,NULL,&_tex);
						ci::app::console()<<hr<<endl;
						ThrowIfFailed(hr);
						mD3D11TextureRead = makeComShared(_tex);

						texDesc.Usage = D3D11_USAGE_DEFAULT;
						texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
						texDesc.CPUAccessFlags = 0;

						hr = mDX11Device->CreateTexture2D(&texDesc,NULL,&_tex2);
						ci::app::console()<<hr<<endl;
						ThrowIfFailed(hr);
						mD3D11Texture = makeComShared(_tex2);

						
						
					}

					


					uint32_t w,h;
					w = size.x;
					h = size.y;
					//make a surface if we don't have one.
					if(!mSurf){
						mSurf = Surface(w,h,true,SurfaceChannelOrder::BGRA);
					}

					
					RECT dst;
					dst.top=0;
					dst.left=0;
					dst.bottom=h;
					dst.right=w;
					
					MFVideoNormalizedRect src;
					src.top=0;
					src.left=0;
					src.bottom=1;
					src.right=1;
					
					//copy the video frame to the write texture
					HRESULT hr = _me()->TransferVideoFrame((mD3D11Texture.get()),&src,&dst,&backColor);
					ThrowIfFailed(hr);

					//Copy the Write texture to the read texture.
					mDX11DeviceContext->CopySubresourceRegion1(mD3D11TextureRead.get(),0,0,0,0,mD3D11Texture.get(),0,NULL,0);
					mDX11DeviceContext->Flush();
					
					
					//Map the Read Texture and copy the data to the cinder surface.
					mDX11DeviceContext->Map(mD3D11TextureRead.get(),0,D3D11_MAP_READ,0,&mMapped);
					void* l_data = mSurf.getData();
					stringstream stm;
					stm<<"width ="<<getNativeVideoSize().x<<" pitch="<<mMapped.RowPitch;
					OutputDebugStringA(stm.str().c_str());
					int len = mSurf.getWidth()*mSurf.getHeight()*mSurf.getPixelInc();
					parallel_for((uint32_t)0,h,[=](uint32_t row){
						byte* dest = ((byte*)l_data)+(row*mSurf.getRowBytes());
						byte* src = ((byte*)mMapped.pData)+(row*mMapped.RowPitch);
						memcpy(dest,src,mSurf.getWidth()*mSurf.getPixelInc());
					});
					//memcpy(l_data,mMapped.pData,len);
					mDX11DeviceContext->Unmap(mD3D11TextureRead.get(),0);
					
				}
			}

			LeaveCriticalSection(&mCritSec);
		}
		else break;
	}

	return 0;
}


ci::gl::Texture& MediaEngine::Obj::getTexture()
{

	return mTex;
}


MediaEngine::TimeRange MediaEngine::Obj::toTimeRange(IMFMediaTimeRange* _timerange,bool release/*=true*/)
{
	MediaEngine::TimeRange result;
	double start;
	double end;
	for(int i=0;i<_timerange->GetLength();i++){
		_timerange->GetStart(i,&start);
		_timerange->GetEnd(i,&end);
		result.addRange(start,end);
	}
	if(release)
		_timerange->Release();
	return result;
}

void MediaEngine::TimeRange::addRange(double startTime,double endTime)
{
	RangeType t(startTime,endTime);
	mRangesList.push_back(t);
}

void MediaEngine::TimeRange::clear()
{
	mRangesList.clear();
}

bool MediaEngine::TimeRange::containsTime(double time){

	for(int i=0;i<mRangesList.size();i++)
	{
		RangeType t = mRangesList[i];
		if(get<0>(t)<=time && get<1>(t)>=time){
			return true;
		}
	}
	return false;
}

double MediaEngine::TimeRange::getEnd(int index)
{
	RangeType t = mRangesList[index];
	return get<1>(t);
}

int32_t MediaEngine::TimeRange::getLength()
{
	return mRangesList.size();
}

double MediaEngine::TimeRange::getStart(int index)
{
	RangeType t = mRangesList[index];
	return get<0>(t);
}