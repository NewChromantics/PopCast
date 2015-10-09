#import "AvfPixelBuffer.h"
#include <SoyOpengl.h>




std::string Soy::Platform::GetExtensions(CMFormatDescriptionRef FormatDescription)
{
	auto Extensions = (NSDictionary*)CMFormatDescriptionGetExtensions( FormatDescription );
	
	std::stringstream Output;
	
	for ( NSString* Key in Extensions )
	{
		Output << Soy::NSStringToString( Key ) << "=";
		
		@try
		{
			NSString* Value = [[Extensions objectForKey:Key] description];
			Output << Soy::NSStringToString( Value );
		}
		@catch (NSException* e)
		{
			Output << "<unkown value " << Soy::NSErrorToString( e ) << ">";
		}
		Output << ", ";
	}
	return Output.str();
}


bool GetExtension(CMFormatDescriptionRef FormatDescription,const std::string& Key,bool& Value)
{
	auto Extensions = (NSDictionary*)CMFormatDescriptionGetExtensions( FormatDescription );
	
	NSString* KeyNs = Soy::StringToNSString( Key );
	
	@try
	{
		NSString* DictValue = [[Extensions objectForKey:KeyNs] description];
		std::stringstream ValueStr;
		ValueStr << Soy::NSStringToString( DictValue );
		return Soy::StringToType( Value, ValueStr.str() );
	}
	@catch (NSException* e)
	{
		std::Debug << "<unkown value " << Soy::NSErrorToString( e ) << ">" << std::endl;
		return false;
	}
}


bool GetExtension(CMFormatDescriptionRef FormatDescription,const std::string& Key,std::string& Value)
{
	auto Extensions = (NSDictionary*)CMFormatDescriptionGetExtensions( FormatDescription );
	
	NSString* KeyNs = Soy::StringToNSString( Key );
	
	@try
	{
		NSString* DictValue = [[Extensions objectForKey:KeyNs] description];
		Value = Soy::NSStringToString( DictValue );
		return true;
	}
	@catch (NSException* e)
	{
		std::Debug << "<unkown value " << Soy::NSErrorToString( e ) << ">" << std::endl;
		return false;
	}
}


std::string GetH264ProfileName(uint8 AtomProfileIdc,uint8 AtomProfileIop,uint8 AtomLevel)
{
	//	http://stackoverflow.com/questions/21120717/h-264-video-wont-play-on-ios
	std::stringstream Name;
	if ( AtomProfileIdc == 0x42 )
		Name << "Baseline Profile";
	else if ( AtomProfileIdc == 0x4D )
		Name << "Main Profile";
	else if ( AtomProfileIdc == 0x58 )
		Name << "Extended Profile";
	else if ( AtomProfileIdc == 0x58 )
		Name << "High Profile";
	else
		Name << std::hex << AtomProfileIdc << "? Profile";
	
	//	show level
	//	https://github.com/ford-prefect/gst-plugins-bad/blob/master/sys/applemedia/vtdec.c
	//	http://stackoverflow.com/questions/21120717/h-264-video-wont-play-on-ios
	//	value is decimal * 10. Even if the data is bad, this should still look okay
	int Minor = AtomLevel % 10;
	int Major = (AtomLevel-Minor) / 10;
	
	Name << " " << Major << "." << Minor;
	
	//	get Iop options
	/*
		constraint_set0_flag 0 u(1)
	 constraint_set1_flag 0 u(1)
	 constraint_set2_flag 0 u(1)
	 constraint_set3_flag 0 u(1)
	 constraint_set4_flag 0 u(1)
	 constraint_set5_flag 0 u(1)
	 reserved_zero_2bits
	 */
	if ( AtomProfileIop != 0 )
		Name << " constraint ";
	
	for ( int i=0;	i<6;	i++ )
	{
		//	reversed endian
		int Bit = (8-i);
		if ( AtomProfileIop & (1<<Bit) )
			Name << i;
	}
	
	return Name.str();
}




void ExtractAppleEmbeddedData(std::string Atoms,std::map<std::string,std::string>& Dictionary)
{
	//	some kinda wierd {} wrapped key system
	BufferArray<char,10> Whitespace;
	Whitespace.PushBack(' ');
	Whitespace.PushBack('\n');
	Whitespace.PushBack('\t');
	Soy::StringTrimLeft( Atoms, '{' );
	
	while ( !Atoms.empty() )
	{
		Soy::StringTrimLeft( Atoms, GetArrayBridge(Whitespace) );
		
		//	read string until whitespace
		std::string Key = Soy::StringPopUntil( Atoms, ' ' );
		Soy::StringTrimLeft( Atoms, GetArrayBridge(Whitespace) );
		Soy::StringTrimLeft( Atoms, '=' );
		Soy::StringTrimLeft( Atoms, GetArrayBridge(Whitespace) );
		
		std::string Value;
		//	array of bytes
		if ( Atoms[0] == '<' )
		{
			Soy::StringTrimLeft( Atoms, '<' );
			Value = Soy::StringPopUntil( Atoms, '>' );
			Soy::StringTrimLeft( Atoms, '>' );
		}
		else
		{
			//	some other kind that needs special handling
		}
		Value += Soy::StringPopUntil( Atoms, ';' );
		Soy::StringTrimLeft( Atoms, ';' );
		
		Dictionary[Key] = Value;
	}
}

void ExtractAppleEmbeddedData(std::string Atoms,const std::string& Key,ArrayBridge<uint8>&& Data)
{
	std::map<std::string,std::string> Dictionary;
	ExtractAppleEmbeddedData( Atoms, Dictionary );
	
	auto ValueIt = Dictionary.find( Key );
	if ( ValueIt == Dictionary.end() )
	{
		std::stringstream Error;
		Error << "Couldn't find " << Key << " in apple embedded data";
		throw Soy::AssertException( Error.str() );
	}
	
	//	parse hex to bytes
	std::string DataStr = ValueIt->second;
	for ( int i=0;	i<DataStr.length();	i+=2 )
	{
		//	skip spacers
		if ( DataStr[i] == ' ' )
		{
			i++;
			continue;
		}
		Data.PushBack( Soy::HexToByte( DataStr[i+0], DataStr[i+1] ) );
	}
}





std::string Soy::Platform::GetCodec(CMFormatDescriptionRef FormatDescription)
{
	//	gr: use Soy::FourCCToString
	// Get the codec and correct endianness
	auto FourCC = CMFormatDescriptionGetMediaSubType( FormatDescription );
	CMVideoCodecType FormatCodec = CFSwapInt32BigToHost( FourCC );
	std::string CodecStr = Soy::FourCCToString( FormatCodec );
	
	//	for H264 (AVC1) get extended codec info
	if ( CodecStr == "avc1" )
	{
		//	get atom info
		std::string Atoms;
		if ( GetExtension( FormatDescription, "SampleDescriptionExtensionAtoms", Atoms ) )
		{
			try
			{
				//	bjork- plays on 5s
				//	{\n    avcC = <014d4032 ffe1002b 674d4032 96520040 0080dff8 0008000a 84000003 00040000 0300f392 00009896 00016e36 fc6383b4 2c5a2401 000468eb 7352>;\n}
				//	mancity01 starts, but never shows anything on 5s
				//	{\n    avcC = <01640033 ffe1001f 67640033 ac2ca400 f0010fb0 15202020 28000003 00080000 030184ed 0b168901 000568eb 735250>;\n}
				
				//	https://developer.apple.com/library/ios/documentation/NetworkingInternet/Conceptual/StreamingMediaGuide/FrequentlyAskedQuestions/FrequentlyAskedQuestions.html
				
				//	H.264 Baseline Level 3.0, Baseline Level 3.1, Main Level 3.1, and High Profile Level 4.1.
				
				//	extract the avcC data
				Array<uint8> AtomData;
				ExtractAppleEmbeddedData(Atoms,"avcC", GetArrayBridge(AtomData) );
				
				//	first data is profile data
				//	01 required
				//	PP PP profile
				//	LL level
				//	http://stackoverflow.com/questions/21120717/h-264-video-wont-play-on-ios
				Soy::Assert( AtomData.GetSize() >= 4, "Not enough atom data to get h264 profile&level" );
				Soy::Assert( AtomData[0] == 1, "Expected 1 as first atom byte");
				
				CodecStr = "H264";
				CodecStr += " ";
				CodecStr += GetH264ProfileName( AtomData[1], AtomData[2], AtomData[3] );
			}
			catch(std::exception& e)
			{
				std::Debug << "Failed to get H264 atom info: " << e.what() << std::endl;
			}
		}
	}
	
	return CodecStr;
}


std::string Soy::Platform::GetCVReturnString(CVReturn Error)
{
#define CASE(e)	case (e): return #e
	switch ( Error )
	{
			CASE( kCVReturnSuccess );
			CASE( kCVReturnFirst );
			CASE( kCVReturnInvalidArgument );
			CASE( kCVReturnAllocationFailed );
			CASE( kCVReturnInvalidDisplay );
			CASE( kCVReturnDisplayLinkAlreadyRunning );
			CASE( kCVReturnDisplayLinkNotRunning );
			CASE( kCVReturnDisplayLinkCallbacksNotSet );
			CASE( kCVReturnInvalidPixelFormat );
			CASE( kCVReturnInvalidSize );
			CASE( kCVReturnInvalidPixelBufferAttributes );
			CASE( kCVReturnPixelBufferNotOpenGLCompatible );
			CASE( kCVReturnPixelBufferNotMetalCompatible );
			CASE( kCVReturnWouldExceedAllocationThreshold );
			CASE( kCVReturnPoolAllocationFailed );
			CASE( kCVReturnInvalidPoolAttributes );
		default:
		{
			std::stringstream Err;
			Err << "Unknown CVReturn error: " << Error;
			return Err.str();
		}
	}
#undef CASE
}



#if defined(TARGET_IOS)
Opengl::TTexture ExtractPlaneTexture(AvfTextureCache& TextureCache,CFPtr<CVImageBufferRef>& ImageBuffer,CFPtr<CVOpenGLESTextureRef>& TextureRef,CFAllocatorRef& Allocator,size_t PlaneIndex,SoyPixelsFormat::Type Format)
{
	GLenum TextureType = GL_TEXTURE_2D;
	GLenum PixelComponentType = GL_UNSIGNED_BYTE;
	
	//	gr: switch these if's to the right pixelformat mapping
	GLenum InternalFormat = GL_INVALID_VALUE;		//	gr: spent a WEEK trying to get this to work. RGBA is the only one that works. not BGRA! ignore the docs!
	GLenum PixelFormat = GL_INVALID_VALUE;			//	todo: fetch this from image buffer
	size_t Width = 0;
	size_t Height = 0;
	
	if ( Format == SoyPixelsFormat::LumaFull || Format == SoyPixelsFormat::LumaVideo )
	{
		TextureType = GL_LUMINANCE;
		PixelFormat = GL_LUMINANCE;
		Width = CVPixelBufferGetWidthOfPlane( ImageBuffer.mObject, PlaneIndex );
		Height = CVPixelBufferGetHeightOfPlane( ImageBuffer.mObject, PlaneIndex );
	}
	else if ( Format == SoyPixelsFormat::GreyscaleAlpha )
	{
		TextureType = GL_LUMINANCE_ALPHA;
		PixelFormat = GL_LUMINANCE_ALPHA;
		Width = CVPixelBufferGetWidthOfPlane( ImageBuffer.mObject, PlaneIndex );
		Height = CVPixelBufferGetHeightOfPlane( ImageBuffer.mObject, PlaneIndex );
	}
	else
	{
		//	gr: spent a WEEK trying to get this to work. RGBA is the only one that works. not BGRA! ignore the docs!
		//	todo: fetch this from image buffer
		InternalFormat = GL_RGBA;
		PixelFormat = GL_BGRA;
		Width = CVPixelBufferGetWidth( ImageBuffer.mObject );
		Height = CVPixelBufferGetHeight( ImageBuffer.mObject );
	}
	
	auto Result = CVOpenGLESTextureCacheCreateTextureFromImage(	Allocator,
															   TextureCache.mTextureCache.mObject,
															   ImageBuffer.mObject,
															   nullptr,
															   TextureType,
															   InternalFormat,
															   size_cast<GLsizei>(Width),
															   size_cast<GLsizei>(Height),
															   PixelFormat,
															   PixelComponentType,
															   PlaneIndex,
															   &TextureRef.mObject
															   );
	
	if ( Result != kCVReturnSuccess || !TextureRef.mObject )
	{
		auto BytesPerRow = CVPixelBufferGetBytesPerRowOfPlane( ImageBuffer.mObject, PlaneIndex );
		Opengl::IsOkay("Failed to CVOpenGLTextureCacheCreateTextureFromImage",false);
		std::Debug << "Failed to create texture from image " << Soy::Platform::GetCVReturnString(Result) << " bytes per row: " << BytesPerRow << "plane #" << PlaneIndex << " as " << Format << std::endl;
		return Opengl::TTexture();
	}
	
	auto RealTextureType = CVOpenGLESTextureGetTarget( TextureRef.mObject );
	auto RealTextureName = CVOpenGLESTextureGetName( TextureRef.mObject );
	SoyPixelsMeta Meta( Width, Height, SoyPixelsFormat::RGBA );
	
	Opengl::TTexture Texture( RealTextureName, Meta, RealTextureType );
	return Texture;
}
#endif


#if defined(TARGET_IOS)
Opengl::TTexture ExtractNonPlanarTexture(AvfTextureCache& TextureCache,CFPtr<CVImageBufferRef>& ImageBuffer,CFPtr<CVOpenGLESTextureRef>& TextureRef,CFAllocatorRef& Allocator,SoyPixelsFormat::Type ExpectedFormat)
#elif defined(TARGET_OSX)
Opengl::TTexture ExtractNonPlanarTexture(AvfTextureCache& TextureCache,CFPtr<CVImageBufferRef>& ImageBuffer,CFPtr<CVOpenGLTextureRef>& TextureRef,CFAllocatorRef& Allocator)
#endif
{
#if defined(TARGET_IOS)
	
	//	ios can just grab plane 0
	return ExtractPlaneTexture( TextureCache, ImageBuffer, TextureRef, Allocator, 0, ExpectedFormat );
	
#elif defined(TARGET_OSX)
	
	//	http://stackoverflow.com/questions/13933503/core-video-pixel-buffers-as-gl-texture-2d
	auto Result = CVOpenGLTextureCacheCreateTextureFromImage(Allocator,
															 TextureCache.mTextureCache.mObject,
															 ImageBuffer.mObject,
															 nullptr,
															 &TextureRef.mObject);
	
	
	if ( Result != kCVReturnSuccess || !TextureRef.mObject )
	{
		Opengl::IsOkay("Failed to CVOpenGLTextureCacheCreateTextureFromImage",false);
		std::Debug << "Failed to create texture from image " << Soy::Platform::GetCVReturnString(Result) << std::endl;
		return Opengl::TTexture();
	}
	
	auto Width = CVPixelBufferGetWidth( ImageBuffer.mObject );
	auto Height = CVPixelBufferGetHeight( ImageBuffer.mObject );
	auto RealTextureType = CVOpenGLTextureGetTarget( TextureRef.mObject );
	auto RealTextureName = CVOpenGLTextureGetName( TextureRef.mObject );
	
	//	we dont KNOW the internal format, so create a temp texture, then
	//	use it to pull out the real pixel format
	SoyPixelsMeta TmpMeta( Width, Height, SoyPixelsFormat::RGBA );
	Opengl::TTexture TmpTexture( RealTextureName, TmpMeta, RealTextureType );
	GLenum AutoRealTextureType = RealTextureType;
	SoyPixelsMeta Meta = TmpTexture.GetInternalMeta(AutoRealTextureType);
	Opengl::TTexture Texture( RealTextureName, Meta, RealTextureType );
	
	return Texture;
#endif
}






void CFPixelBuffer::Lock(ArrayBridge<Opengl::TTexture>&& Textures,Opengl::TContext& Context)
{
	Opengl::IsOkay("LockTexture flush", false);
	
	Soy::Assert( mDecoder!=nullptr, "Decoder expected" );
	
	auto& Buffer = mSample.mObject;
	mLockedImageBuffer.Retain( CMSampleBufferGetImageBuffer(Buffer) );
	if ( !mLockedImageBuffer )
	{
		std::Debug << "Failed to get ImageBuffer from CMSampleBuffer" << std::endl;
		Unlock();
		return;
	}
	
	auto& ImageBuffer = mLockedImageBuffer.mObject;
	CVReturn Result = CVPixelBufferLockBaseAddress( ImageBuffer, mReadOnlyLock ? kCVPixelBufferLock_ReadOnly : 0 );
	if ( Result != kCVReturnSuccess  )
	{
		Opengl::IsOkay("Failed to lock address",false);
		std::Debug << "Error locking base address of image: " << Soy::Platform::GetCVReturnString(Result) << std::endl;
		Unlock();
		return;
	}
	
	CFAllocatorRef Allocator = kCFAllocatorDefault;
	
	
#if defined(TARGET_IOS)
	
	auto PlaneCount = CVPixelBufferGetPlaneCount( ImageBuffer );
	if ( PlaneCount > 0 )
	{
		BufferArray<SoyPixelsFormat::Type,2> PlaneFormats;
		SoyPixelsFormat::GetFormatPlanes( mFormat, GetArrayBridge(PlaneFormats) );
		for ( int i=0;	i<PlaneCount;	i++ )
		{
			auto TextureCache = mDecoder->GetTextureCache(i);
			if ( !TextureCache )
				throw Soy::AssertException("Failed to get texture cache");
			
			//	hacky
			auto& TextureRef = (i==0) ? mLockedTexture0 : mLockedTexture1;
			
			mTextureCaches.PushBack( TextureCache );
			auto Texture = ExtractPlaneTexture( *TextureCache, mLockedImageBuffer, TextureRef, Allocator, i, PlaneFormats[i] );
			if ( !Texture.IsValid() )
				continue;
			
			Textures.PushBack( Texture );
		}
	}
	else
	{
		auto& TextureRef = mLockedTexture0;
		auto TextureCache = mDecoder->GetTextureCache(0);
		if ( !TextureCache )
			throw Soy::AssertException("Failed to get texture cache");
		
		mTextureCaches.PushBack( TextureCache );
		auto Texture = ExtractNonPlanarTexture( *TextureCache, mLockedImageBuffer, TextureRef, Allocator, mFormat );
		
		if ( Texture.IsValid() )
			Textures.PushBack( Texture );
	}
#elif defined(TARGET_OSX)
	
	//	on OSX, we can't pull multiple planes into opengl textures (get the "incompatible with opengl error")
	//	so we don't return anything. Caller has to use pixels -> texture instead
	auto PlaneCount = CVPixelBufferGetPlaneCount( ImageBuffer );
	if ( PlaneCount > 0 )
	{
		Unlock();
		return;
	}
	
	auto TextureCache = mDecoder->GetTextureCache(0);
	if ( !TextureCache )
		throw Soy::AssertException("Failed to get texture cache");
	
	mTextureCaches.PushBack( TextureCache );
	auto Texture = ExtractNonPlanarTexture( *TextureCache, mLockedImageBuffer, mLockedTexture, Allocator );
	
	if ( Texture.IsValid() )
		Textures.PushBack( Texture );
#endif
}




CFPixelBuffer::~CFPixelBuffer()
{
	//	gotta WAIT for this to unlock from the other thread! NOT unlock it ourselves
	WaitForUnlock();
	
	//auto RetainCount = mSample.GetRetainCount();
	//std::Debug << "Sample has " << RetainCount << " references in " << __func__ << std::endl;
	
	static bool invalidate = false;
	if ( mSample && invalidate )
		CMSampleBufferInvalidate(mSample.mObject);
	mSample.Release();
}


CVImageBufferRef CFPixelBuffer::LockImageBuffer()
{
	Soy::Assert( mLockedImageBuffer == false, "Image buffer already locked");
	
	mLockedImageBuffer.Retain( CMSampleBufferGetImageBuffer(mSample.mObject) );
	if ( mLockedImageBuffer )
		return mLockedImageBuffer.mObject;
	
	//	debug why it failed
	std::Debug << "Failed to get ImageBuffer from CMSampleBuffer... ";
	
	auto DataIsReady = CMSampleBufferDataIsReady( mSample.mObject );
	std::Debug << "Data is ready: " << DataIsReady << ", ";
	
	
	CMFormatDescriptionRef FormatDescription = CMSampleBufferGetFormatDescription( mSample.mObject );
	if ( FormatDescription == nullptr )
		std::Debug << "Format description: null,";
	else
		std::Debug << "Format description: not null,";
	
	std::Debug << std::endl;
	
	return mLockedImageBuffer.mObject;
}

void CFPixelBuffer::Lock(ArrayBridge<SoyPixelsImpl*>&& Planes)
{
	mLockLock.lock();
	
	//	reset
	mLockedPixels.SetAll( SoyPixelsRemote() );
	
	auto PixelBuffer = LockImageBuffer();
	if ( !PixelBuffer )
	{
		Unlock();
		return;
	}
	
	auto Error = CVPixelBufferLockBaseAddress( PixelBuffer, mReadOnlyLock ? kCVPixelBufferLock_ReadOnly : 0 );
	if ( Error != kCVReturnSuccess )
	{
		std::Debug << "Failed to lock CVPixelBuffer address " << Soy::Platform::GetCVReturnString( Error ) << std::endl;
		Unlock();
		return;
	}
	
	//	here we diverge for multiple planes
	auto PlaneCount = CVPixelBufferGetPlaneCount( PixelBuffer );
	if ( PlaneCount >= 1 )
	{
		BufferArray<SoyPixelsFormat::Type,2> PlaneFormats;
		SoyPixelsFormat::GetFormatPlanes( mFormat, GetArrayBridge(PlaneFormats) );
		for ( size_t PlaneIndex=0;	PlaneIndex<PlaneCount;	PlaneIndex++ )
		{
			auto Width = CVPixelBufferGetWidthOfPlane( PixelBuffer, PlaneIndex );
			auto Height = CVPixelBufferGetHeightOfPlane( PixelBuffer, PlaneIndex );
			auto* Pixels = CVPixelBufferGetBaseAddressOfPlane( PixelBuffer, PlaneIndex );
			if ( !Pixels )
			{
				std::Debug << "Image plane #" << PlaneIndex << "/" << PlaneCount << " " << Width << "x" << Height << " return null" << std::endl;
				continue;
			}
			
			auto PlaneFormat = PlaneFormats[PlaneIndex];
			std::Debug << "Image plane #" << PlaneIndex << "/" << PlaneCount << " " << Width << "x" << Height << std::endl;
			
			//	data size here is for the whole image...
			auto DataSize = CVPixelBufferGetDataSize(PixelBuffer);
			SoyPixelsRemote Temp( reinterpret_cast<uint8*>(Pixels), Width, Height, DataSize, PlaneFormat );
			mLockedPixels[PlaneIndex] = Temp;
			Planes.PushBack( &mLockedPixels[PlaneIndex] );
		}
	}
	else
	{
		//	get the "non-planar" image
		auto Height = CVPixelBufferGetHeight( PixelBuffer );
		auto Width = CVPixelBufferGetWidth( PixelBuffer );
		auto* Pixels = CVPixelBufferGetBaseAddress(PixelBuffer);
		if ( !Pixels )
		{
			Unlock();
			return;
		}
		
		auto DataSize = CVPixelBufferGetDataSize(PixelBuffer);
		SoyPixelsRemote Temp( reinterpret_cast<uint8*>(Pixels), Width, Height, DataSize, mFormat );
		mLockedPixels[0] = Temp;
		Planes.PushBack( &mLockedPixels[0] );
	}
}


void CFPixelBuffer::Unlock()
{
	//	release our use of the texture cache
	auto ClearTextureCache = [](std::shared_ptr<AvfTextureCache>& Cache)
	{
		if ( Cache )
			Cache->Flush();
		Cache.reset();
		return true;
	};
	GetArrayBridge(mTextureCaches).ForEach(ClearTextureCache);
	mTextureCaches.Clear();
	
#if defined(TARGET_IOS)
	if ( mLockedTexture0 )
		mLockedTexture0.Release();
	if ( mLockedTexture1 )
		mLockedTexture1.Release();
#elif defined(TARGET_OSX)
	if ( mLockedTexture )
		mLockedTexture.Release();
#endif
	mLockedPixels.SetAll( SoyPixelsRemote() );
	
	if ( mLockedImageBuffer )
	{
		//	must make sure nothing is using texture before releasing it
		//glFlush();
		CVPixelBufferUnlockBaseAddress(mLockedImageBuffer.mObject, mReadOnlyLock ? kCVPixelBufferLock_ReadOnly : 0 );
		
		//std::Debug << "[c] Locked image buffer refcount before release: " << mLockedImageBuffer.GetRetainCount() << std::endl;
		mLockedImageBuffer.Release();
	}
	mLockLock.unlock();
}

void CFPixelBuffer::WaitForUnlock()
{
	while ( !mLockLock.try_lock() )
	{
		std::Debug << "CFPixelBuffer: waiting for image buffer to unlock" << std::endl;
	}
	mLockLock.unlock();
}



#if defined(TARGET_IOS)
__export EAGLContext*	UnityGetMainScreenContextGLES();
//extern EAGLContext*	UnityGetContextEAGL();
#endif

AvfTextureCache::AvfTextureCache()
{
	CFAllocatorRef Allocator = kCFAllocatorDefault;
	
#if defined(TARGET_IOS)
	
	auto Context = UnityGetMainScreenContextGLES();
	CVOpenGLESTextureCacheRef Cache;
	auto Result = CVOpenGLESTextureCacheCreate ( Allocator, nullptr, Context, nullptr, &Cache );
	
#elif defined(TARGET_OSX)
	
	auto Context = CGLGetCurrentContext();
	//	cannot pass null pixelformat like we can on IOS
	//	gr: not retained... assuming okay for this small amount of time
	CGLPixelFormatObj PixelFormat = CGLGetPixelFormat( Context );
	CVOpenGLTextureCacheRef Cache = nullptr;
	auto Result = CVOpenGLTextureCacheCreate ( Allocator, nullptr, Context, PixelFormat, nullptr, &Cache );
	
#endif
	
	//	gr: unnecessary additional retain?
	static bool AdditionalRetain = false;
	if ( AdditionalRetain )
		mTextureCache.Retain( Cache );
	else
		mTextureCache.SetNoRetain( Cache );
	
	
	Opengl::IsOkay("Create texture cache flush", false);
	
	if ( Result != kCVReturnSuccess )
	{
		std::stringstream Error;
		Error << "Failed to allocate texture cache " << Soy::Platform::GetCVReturnString(Result);
		throw Soy::AssertException( Error.str() );
	}
}


AvfTextureCache::~AvfTextureCache()
{
	Flush();
	mTextureCache.Release();
}


void AvfTextureCache::Flush()
{
	//	shouldn't really occur
	if ( !mTextureCache )
		return;
	
	//	gotta make sure all uses of texture are done before flushing
	//	gr: might not have an opengl context! must move this to the last-texture use!
	//glFlush();
#if defined(TARGET_IOS)
	CVOpenGLESTextureCacheFlush( mTextureCache.mObject, 0 );
#elif defined(TARGET_OSX)
	CVOpenGLTextureCacheFlush( mTextureCache.mObject, 0 );
#endif
	
}



std::shared_ptr<AvfTextureCache> AvfDecoderRenderer::GetTextureCache(size_t Index)
{
	if ( Index >= mTextureCaches.GetSize() )
		mTextureCaches.SetSize( Index+1 );
	
	auto& TextureCache = mTextureCaches[Index];
	if ( TextureCache )
		return TextureCache;
	
	try
	{
		TextureCache.reset( new AvfTextureCache );
	}
	catch (std::exception& e)
	{
		std::Debug << "Failed to create AvfTextureCache: " << e.what() << std::endl;
		TextureCache.reset();
	}
	
	return TextureCache;
}

