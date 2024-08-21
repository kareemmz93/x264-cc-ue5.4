
// Fill out your copyright notice in the Description page of Project Settings.


#include "CameraCapture.h"
#include "Engine/TextureRenderTarget2D.h"

// Sets default values
ACameraCapture::ACameraCapture()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	//SceneCapture->SetupAttachment(RootComponent);
	//SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	//RenderTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("RenderTarget"));
	//RenderTarget->InitAutoFormat(Width, Height);
	//SceneCapture->TextureTarget = RenderTarget;
	
}

// Called when the game starts or when spawned
void ACameraCapture::BeginPlay()
{
	Super::BeginPlay();

	// Create UDP socket
	FIPv4Address LocalAddress;
	FIPv4Address::Parse(TEXT("0.0.0.0"), LocalAddress);
	TSharedPtr<FInternetAddr> LocalEndpoint = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	LocalEndpoint->SetIp(LocalAddress.Value);
	LocalEndpoint->SetPort(0);

	UDPSocket = FUdpSocketBuilder(TEXT("MyUDPClientSocket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToAddress(LocalAddress)
		.BoundToPort(0);

	// Set up remote endpoint
	FIPv4Address RemoteAddress;
	FIPv4Address::Parse(TEXT("192.168.130.33"), RemoteAddress);
	//TSharedPtr<FInternetAddr> RemoteEndpoint = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	//RemoteEndpoint->SetIp(RemoteAddress.Value);
	//RemoteEndpoint->SetPort(5000);
	this->RemoteEndpoint = FIPv4Endpoint(RemoteAddress, 5000);

	SceneCapture->SetupAttachment(RootComponent);
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	RenderTarget->InitAutoFormat(Width, Height);
	SceneCapture->TextureTarget = RenderTarget;
	
	IsSendable = x264Initialize();

	UE_LOG(LogTemp, Log, TEXT("=>Initialized"));
}


bool ACameraCapture::x264Initialize() {

	x264_param_t param;

	// Initialize x264 parameters
	x264_param_default_preset(&param, "veryfast", "zerolatency");
	param.i_width = Width;
	param.i_height = Height;
	param.i_csp = X264_CSP_I420;
	param.i_bframe = 2;


	// Apply profile (optional but recommended for certain use cases)
	if (x264_param_apply_profile(&param, "main") < 0) {
		UE_LOG(LogTemp, Error, TEXT("Failed to apply x264 profile"));
		return false;
	}

	// Allocate picture for encoding
	if (x264_picture_alloc(&pic_in, param.i_csp, param.i_width, param.i_height) < 0) {
		UE_LOG(LogTemp, Error, TEXT("Failed to allocate picture"));
		return false;
	}

	// Open x264 encoder
	encoder = x264_encoder_open(&param);
	if (!encoder) {
		UE_LOG(LogTemp, Error, TEXT("Failed to open x264 encoder"));
		return false;
	}

	return true;

}


void ACameraCapture::OnDestroyed()
{
	// Clean up
	if (encoder) {
		x264_encoder_close(encoder);
	}
	x264_picture_clean(&pic_in);
	x264_picture_clean(&pic_out);
	UDPSocket->Close();
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(UDPSocket);
}

// Called every frame
void ACameraCapture::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsSendable) {
		SentImageOverUdp();
	}
}

void ACameraCapture::SentImageOverUdp() {

	if (RenderTarget == nullptr) {
		UE_LOG(LogTemp, Log, TEXT("%d"), RenderTarget == nullptr ? 0 : 1);
		return;
	}

	FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();

	TArray<FColor> OutBMP;
	RenderTargetResource->ReadPixels(OutBMP);

	TArray<uint8> YUVData;
	ConvertRGBToYUV420(OutBMP, Width, Height, YUVData);

	TArray<uint8> EncodedData;
	EncodeImageWithx264(YUVData, Width, Height, EncodedData);

	//TArray<uint8> CompressedBMP;
	//FImageUtils::CompressImageArray(RenderTarget->SizeX, RenderTarget->SizeY, OutBMP, CompressedBMP);
	// Send data

	int32 BytesSent = 0;
	UDPSocket->SendTo(EncodedData.GetData(), EncodedData.Num(), BytesSent, *RemoteEndpoint.ToInternetAddr());
	UE_LOG(LogTemp, Log, TEXT("%d bytes was sent"), BytesSent);

}


void ACameraCapture::OnReceive()
{
	if (UDPSocket)
	{
		TArray<uint8> ReceivedData;
		int32 Size;
		while (UDPSocket->RecvFrom(ReceivedData.GetData(), ReceivedData.Num(), Size, *RemoteEndpoint.ToInternetAddr()))
		{
			FString ReceivedMessage = FString(UTF8_TO_TCHAR(ReceivedData.GetData()));
			UE_LOG(LogTemp, Log, TEXT("Received message: %s"), *ReceivedMessage);
		}
	}
}

void ACameraCapture::EncodeImageWithx264(const TArray<uint8>& YUVData, int32 _Width, int32 _Height, TArray<uint8>& EncodedData)
{
	
	x264_nal_t* nal = nullptr;
	int i_nal = 0;
	int frame_size = 0;

	// Ensure YUVData is valid
	if (YUVData.Num() < (_Width * _Height) + 2 * (_Width / 2) * (_Height / 2)) {
		UE_LOG(LogTemp, Error, TEXT("YUVData size is incorrect"));
		goto fail;
	}

	// Copy YUV data to x264 picture
	FMemory::Memcpy(pic_in.img.plane[0], YUVData.GetData(), _Width * _Height); // Y plane
	FMemory::Memcpy(pic_in.img.plane[1], YUVData.GetData() + _Width * _Height, _Width * _Height / 4); // U plane
	FMemory::Memcpy(pic_in.img.plane[2], YUVData.GetData() + _Width * _Height + _Width * _Height / 4, _Width * _Height / 4); // V plane

	pic_in.i_pts = FrameNum;

	// Encode the image
	frame_size = x264_encoder_encode(encoder, &nal, &i_nal, &pic_in, &pic_out);
	if (frame_size < 0) {
		UE_LOG(LogTemp, Error, TEXT("x264 encoding failed"));
		goto fail;
	}

	UE_LOG(LogTemp, Warning, TEXT("Frame size: %d"), frame_size);

	if (frame_size > 0) {
		EncodedData.SetNumUninitialized(frame_size);
		FMemory::Memcpy(EncodedData.GetData(), nal[0].p_payload, frame_size);
	}

fail:
	;
	// Clean up
	//if (encoder) {
	//	x264_encoder_close(encoder);
	//}
	//x264_picture_clean(&pic_in);
}


void ACameraCapture::ConvertRGBToYUV420(const TArray<FColor>& RGBData, int32 _Width, int32 _Height, TArray<uint8>& YUVData)
{
	int YSize = _Width * _Height;
	int UVSize = (_Width / 2) * (_Height / 2); // U and V planes are half the size of Y
	YUVData.SetNumUninitialized(YSize + 2 * UVSize);

	uint8* YPlane = YUVData.GetData();
	uint8* UPlane = YPlane + YSize;
	uint8* VPlane = UPlane + UVSize;

	// Initialize UV planes
	TArray<uint8> UPlaneTemp;
	TArray<uint8> VPlaneTemp;
	UPlaneTemp.SetNumUninitialized(UVSize);
	VPlaneTemp.SetNumUninitialized(UVSize);

	int32 UVIndex = 0;
	for (int32 y = 0; y < _Height; ++y)
	{
		for (int32 x = 0; x < _Width; ++x)
		{
			int32 Index = y * _Width + x;
			FColor Color = RGBData[Index];
			uint8 R = Color.R;
			uint8 G = Color.G;
			uint8 B = Color.B;

			// Convert RGB to YUV
			uint8 Y = (uint8)(0.299f * R + 0.587f * G + 0.114f * B);
			uint8 U = (uint8)(-0.14713f * R - 0.28886f * G + 0.436f * B + 128);
			uint8 V = (uint8)(0.615f * R - 0.51499f * G - 0.10001f * B + 128);

			// Assign Y component
			YPlane[Index] = Y;

			// Chroma subsampling: Average U and V values for 2x2 blocks
			if ((x % 2 == 0) && (y % 2 == 0))
			{
				// Get U and V components
				uint8 UBlock[4] = { U, U, U, U };
				uint8 VBlock[4] = { V, V, V, V };
				for (int32 dy = 0; dy < 2; ++dy)
				{
					for (int32 dx = 0; dx < 2; ++dx)
					{
						int32 IndexUV = ((y / 2) * (Width / 2)) + (x / 2);
						UPlaneTemp[IndexUV] = UBlock[dy * 2 + dx];
						VPlaneTemp[IndexUV] = VBlock[dy * 2 + dx];
					}
				}
			}
		}
	}

	// Copy U and V planes to the output buffer
	FMemory::Memcpy(UPlane, UPlaneTemp.GetData(), UVSize);
	FMemory::Memcpy(VPlane, VPlaneTemp.GetData(), UVSize);
}
