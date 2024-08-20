// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget2D.h"
#include <Components/SceneCaptureComponent2D.h>
#include "ImageUtils.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "x264.h"
#include "CameraCapture.generated.h"


UCLASS()
class MYPROJECT_API ACameraCapture : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACameraCapture();
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	void OnDestroyed();
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;


	void SentImageOverUdp();
	void OnReceive();
	bool x264Initialize();
	void ConvertRGBToYUV420(const TArray<FColor>& RGBData, int32 Width, int32 Height, TArray<uint8>& YUVData);
	void EncodeImageWithx264(const TArray<uint8>& YUVData, int32 Width, int32 Height, TArray<uint8>& EncodedData);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	USceneCaptureComponent2D* SceneCapture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	UTextureRenderTarget2D* RenderTarget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	int Width = 640;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	int Height = 480;

	FSocket* UDPSocket;
	FIPv4Endpoint RemoteEndpoint;
	bool IsSendable = false;

private:
	x264_t* encoder = nullptr;
	x264_picture_t pic_in, pic_out;
	int FrameNum = 0;
};
