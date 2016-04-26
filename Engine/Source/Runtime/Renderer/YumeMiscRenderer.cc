//----------------------------------------------------------------------------
//Yume Engine
//Copyright (C) 2015  arkenthera
//This program is free software; you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation; either version 2 of the License, or
//(at your option) any later version.
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//You should have received a copy of the GNU General Public License along
//with this program; if not, write to the Free Software Foundation, Inc.,
//51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.*/
//----------------------------------------------------------------------------
//
// File : <Filename>
// Date : <Date>
// Comments :
//
//----------------------------------------------------------------------------
#include "YumeHeaders.h"
#include "YumeMiscRenderer.h"

#include "LPVRendererTest.h"

#include "YumeRHI.h"
#include "YumeVertexBuffer.h"
#include "YumeIndexBuffer.h"

#include "YumePostProcess.h"
#include "YumeTimer.h"

#include "Engine/YumeEngine.h"

#include "RenderPass.h"
#include "Scene.h"
#include "SceneNode.h"
#include "Light.h"
#include "LightPropagationVolume.h"

using namespace DirectX;

#define DEBUGGING_RSM
#define DISABLE_CALLS

namespace YumeEngine
{
	struct SimpleVertex
	{
		DirectX::XMFLOAT3 V;
	};

	YumeMiscRenderer::YumeMiscRenderer()
		:
		zNear(.2f),
		zFar(100000.f),
		giEnabled_(true),
		updateRsm_(true),
		rsmSize(1024)
	{
		rhi_ = gYume->pRHI ;

		gYume->pEngine->AddListener(this);

		scene_ = YumeAPINew Scene;

		giParams_.DebugView = false;
		giParams_.LPVFlux = 4;
		giParams_.Scale = 1.0f;
	}

	YumeMiscRenderer::~YumeMiscRenderer()
	{

	}
	void YumeMiscRenderer::Initialize()
	{
		camera_ = YumeAPINew YumeLPVCamera;
		camera_->SetViewParams(XMVectorSet(0,5,-10,1),XMVectorSet(0,0,0,0));
		camera_->SetProjParams(60.0f * M_DEGTORAD,1600.0f / 900.0f,zNear,zFar);
		camera_->SetScalers(0.0099f,0.6f);

		defaultPass_ = YumeAPINew RenderPass;

		defaultPass_->Load("RenderCalls/ReflectiveShadowMap.xml");

		Setup();

		if(GetGIEnabled())
		{
			lightPropagator_.Create(32);

			curr_ = 0;
			next_ = 1;

			num_iterations_rendered = 0;
		}

#ifndef DISABLE_CALLS
		RenderCallPtr clearCall = YumeAPINew RenderCall(CallType::CLEAR);
		clearCall->SetPassName("RSMClear");

		RenderCallPtr rsmRender = YumeAPINew RenderCall(CallType::SCENE,"LPV/Mesh","LPV/Mesh","","MeshVs","MeshPs");
		rsmRender->SetShadowPass(true);

		rsmRender->SetPassName("ReflectiveShadowMapPass");
		rsmRender->SetMiscRenderingFlags(RF_NOBLENDSTATE);

		RenderTargetDesc dummyRt; //baka
		ZeroMemory(&dummyRt,sizeof(RenderTargetDesc));

		dummyRt.Width = rsmSize;
		dummyRt.Height = rsmSize;
		dummyRt.ArraySize = 1;
		dummyRt.Format = gYume->pRHI->GetRGBAFloat16FormatNs();
		dummyRt.Name = "RSM_COLORS";
		dummyRt.Usage = TEXTURE_RENDERTARGET;
		dummyRt.Mips = 1;
		dummyRt.Index = 0;
		dummyRt.Type = RT_OUTPUT;
		dummyRt.ClearColor = YumeColor(0,0,0,1);
		Texture2DPtr RsmColors = clearCall->AddTexture(dummyRt);

		dummyRt.Index = 1;
		dummyRt.Name = "RSM_NORMALS";
		dummyRt.Format = 24;
		Texture2DPtr RsmNormals = clearCall->AddTexture(dummyRt);

		dummyRt.Index = 2;
		dummyRt.Format = 16;
		dummyRt.Name = "RSM_LINEARDEPTH";
		dummyRt.Mips = 0;
		dummyRt.ClearColor = YumeColor(10000,10000,10000,10000);
		Texture2DPtr RsmLinearDepth = clearCall->AddTexture(dummyRt);

		dummyRt.Index = 3;
		dummyRt.Name = "RSM_DUMMYSPEC";
		dummyRt.Format = gYume->pRHI->GetRGBAFloat16FormatNs();
		dummyRt.ClearColor = YumeColor(0,0,0,1);
		dummyRt.Mips = 1;
		Texture2DPtr RsmSpec = clearCall->AddTexture(dummyRt);

		dummyRt.Usage = TEXTURE_DEPTHSTENCIL;
		dummyRt.Type = RT_DEPTHSTENCIL;
		dummyRt.Format = 39;
		dummyRt.Name = "RSM_DSV";
		Texture2DPtr depthStencil = clearCall->AddTexture(dummyRt);


		clearCall->SetClearFlags(CLEAR_DEPTH);

		YumeTexture2D* textures[5] ={RsmColors,RsmNormals,RsmLinearDepth,RsmSpec,depthStencil};

		rsmRender->AddTextures(5,textures);


		defaultPass_->AddRenderCall(clearCall);
		defaultPass_->AddRenderCall(rsmRender);

		RenderCallPtr gbufferCall = YumeAPINew RenderCall(CallType::SCENE,"LPV/Mesh","LPV/Mesh","","MeshVs","MeshPs");
		gbufferCall->SetPassName("GBufferPass");

		RenderTargetDesc gBufferTargets; //baka
		ZeroMemory(&gBufferTargets,sizeof(RenderTargetDesc));

		gBufferTargets.Width = rhi_->GetWidth();
		gBufferTargets.Height = rhi_->GetHeight();
		gBufferTargets.ArraySize = 1;
		gBufferTargets.Format = gYume->pRHI->GetRGBAFloat16FormatNs();
		gBufferTargets.Name = "SCENE_COLORS";
		gBufferTargets.Usage = TEXTURE_RENDERTARGET;
		gBufferTargets.Mips = 1;
		gBufferTargets.Index = 0;
		gBufferTargets.Type = RT_OUTPUT | RT_INPUT;
		gBufferTargets.ClearColor = YumeColor(0,0,0,1);

		Texture2DPtr SceneColors = gbufferCall->AddTexture(gBufferTargets);

		gBufferTargets.Name = "SCENE_NORMALS";
		gBufferTargets.Format = 24; ////DXGI_FORMAT_R10G10B10A2_UNORM
		gBufferTargets.Index = 1;
		Texture2DPtr SceneNormals = gbufferCall->AddTexture(gBufferTargets);

		gBufferTargets.Name = "SCENE_SPECULAR";
		gBufferTargets.Format = gYume->pRHI->GetRGBAFloat16FormatNs();; ////DXGI_FORMAT_R10G10B10A2_UNORM
		gBufferTargets.Index = 3;
		Texture2DPtr SceneSpecular = gbufferCall->AddTexture(gBufferTargets);

		gBufferTargets.Name = "SCENE_LINEARDEPTH";
		gBufferTargets.Format = 16;
		gBufferTargets.Index = 2;
		gBufferTargets.Mips = 0;
		Texture2DPtr SceneLd = gbufferCall->AddTexture(gBufferTargets);

		Setup();

		if(GetGIEnabled())
		{
			lightPropagator_.Create(32);

			curr_ = 0;
			next_ = 1;

			num_iterations_rendered = 0;
		}

		//It is important to add gbuffer call after GI calls
		defaultPass_->AddRenderCall(gbufferCall);

		RenderCallPtr genMips = YumeAPINew RenderCall(CallType::GENERATEMIPS);

		genMips->AddTexture(RT_INPUT,0,RsmLinearDepth);
		genMips->AddTexture(RT_INPUT,1,SceneLd);

		defaultPass_->AddRenderCall(genMips);

		RenderCallPtr fsFinal = YumeAPINew RenderCall(CallType::FSTRIANGLE,"","LPV/deferred_lpv","","","deferred_lpv_ps");
		fsFinal->SetMiscRenderingFlags(RF_NODEPTHSTENCIL);
		//Add Inputs
		fsFinal->AddTexture(RT_INPUT,0,SceneColors);
		fsFinal->AddTexture(RT_INPUT,1,SceneSpecular);
		fsFinal->AddTexture(RT_INPUT,2,SceneNormals);
		fsFinal->AddTexture(RT_INPUT,3,SceneLd);
		fsFinal->AddTexture(RT_INPUT,4,RsmLinearDepth);

		RenderCallPtr lpvInject = defaultPass_->GetCallByName("LPVInject");

		Texture2DPtr accumR = lpvInject->GetInput(6);
		Texture2DPtr accumG = lpvInject->GetInput(7);
		Texture2DPtr accumB = lpvInject->GetInput(8);

		fsFinal->AddTexture(RT_INPUT,5,accumR);
		fsFinal->AddTexture(RT_INPUT,6,accumG);
		fsFinal->AddTexture(RT_INPUT,7,accumB);

		//fsFinal->SetSampler(PS,2,2);
		/*fsFinal->SetSampler(PS,1,3);*/

		fsFinal->SetShaderParameter("gi_scale",giParams_.Scale);
		fsFinal->SetShaderParameter("lpv_flux_amplifier",giParams_.LPVFlux);
		fsFinal->SetShaderParameter("debug_gi",giParams_.DebugView);
		fsFinal->SetShaderParameter("scene_dim_max",XMFLOAT4(GetMaxBb().x,GetMaxBb().y,GetMaxBb().z,1.0f));
		fsFinal->SetShaderParameter("scene_dim_min",XMFLOAT4(GetMinBb().x,GetMinBb().y,GetMinBb().z,1.0f));

		fsFinal->SetShaderParameter("lpv_size",32.0f);
		fsFinal->SetShaderParameter("light_vp",XMMatrixIdentity());
		fsFinal->SetShaderParameter("light_vp_inv",XMMatrixIdentity());
		fsFinal->SetShaderParameter("light_mvp",XMMatrixIdentity());
		fsFinal->SetShaderParameter("light_vp_tex",XMMatrixIdentity());



		defaultPass_->AddRenderCall(fsFinal);

		RenderTargetDesc frontBuffer;
		frontBuffer.Width = 1600;
		frontBuffer.Height = 900;
		frontBuffer.ArraySize = 1;
		frontBuffer.Mips = 10;
		frontBuffer.Name = "FrontBuffer";
		frontBuffer.Format = gYume->pRHI->GetRGBAFloat16FormatNs();
		frontBuffer.Usage = TEXTURE_RENDERTARGET;
		frontBuffer.Type = RT_OUTPUT;
		frontBuffer.Index = 0;

		/*fsFinal->AddTexture(frontBuffer);*/
		fsFinal->AddTexture(RT_OUTPUT,0,0);
		SharedPtr<YumeVertexBuffer> triangleVb(gYume->pRHI->CreateVertexBuffer());

		SimpleVertex v1 ={DirectX::XMFLOAT3(-1.f,-3.f,1.f)};
		SimpleVertex v2 ={DirectX::XMFLOAT3(-1.f,1.f,1.f)};
		SimpleVertex v3 ={DirectX::XMFLOAT3(3.f,1.f,1.f)};

		SimpleVertex vertices[3] ={v1,v2,v3};
		triangleVb->SetShadowed(true);
		triangleVb->SetSize(3,MASK_POSITION);
		triangleVb->SetData(vertices);

		fullscreenTriangle_ = SharedPtr<YumeGeometry>(new YumeGeometry);
		fullscreenTriangle_->SetVertexBuffer(0,triangleVb);
		fullscreenTriangle_->SetDrawRange(TRIANGLE_LIST,0,0,0,3);


		rsmRender->SetIdentifier("LPVDeferred");
		clearCall->SetIdentifier("LPVDeferred");

		SamplerStateDesc lpvFilters;
		lpvFilters.Filter = MinMagMipLinear;
		lpvFilters.AddressModeU = TAM_Clamp;
		lpvFilters.AddressModeV = TAM_Clamp;
		lpvFilters.AddressModeW = TAM_Clamp;
		lpvFilters.BorderColor[0] = lpvFilters.BorderColor[1] = lpvFilters.BorderColor[2] = lpvFilters.BorderColor[3] = 0.0f;
		lpvFilters.ComparisonFunc = TCF_NEVER;
		lpvFilters.MaxLOD = D3D11_FLOAT32_MAX; //3.402823466e+38f
		lpvFilters.Name = "ShadowFilter";
		lpvFilters.MaxAniso = 1;
		unsigned shadowSampler = rhi_->CreateSamplerState(lpvFilters);

		lpvFilters.AddressModeU = TAM_Border;
		lpvFilters.AddressModeV = TAM_Border;
		lpvFilters.AddressModeW = TAM_Border;
		lpvFilters.MaxAniso = 1;
		lpvFilters.Name = "LPVFilter";
		unsigned lpvFilter = rhi_->CreateSamplerState(lpvFilters);

		lpvFilters.AddressModeU = TAM_Clamp;
		lpvFilters.AddressModeV = TAM_Clamp;
		lpvFilters.AddressModeW = TAM_Clamp;
		lpvFilters.ComparisonFunc = TCF_ALWAYS;
		lpvFilters.Filter = MinMagMipPoint;
		lpvFilters.MaxAniso = 1;
		lpvFilters.Name = "VPLFilter";
		unsigned vplFilter = rhi_->CreateSamplerState(lpvFilters);

		lpvFilters.Filter = MinMagMipLinear;
		lpvFilters.ComparisonFunc = TCF_NEVER;
		lpvFilters.AddressModeU = TAM_Wrap;
		lpvFilters.AddressModeV = TAM_Wrap;
		lpvFilters.AddressModeW = TAM_Wrap;
		lpvFilters.Name = "StandardFilter";

		unsigned standardFilter = rhi_->CreateSamplerState(lpvFilters);

		fsFinal->SetSampler(PS,1,lpvFilter);
		fsFinal->SetSampler(PS,2,shadowSampler);

		gbufferCall->SetSampler(PS,0,standardFilter);
		lpvInject->SetSampler(VS,0,vplFilter);

		rsmRender->SetSampler(PS,0,standardFilter);
#endif

	}

	void YumeMiscRenderer::Setup()
	{
	}

	void YumeMiscRenderer::Update(float timeStep)
	{
		if(!gYume->pInput->GetMouseButtonDown(MOUSEB_LEFT))
			camera_->FrameMove(timeStep);



		Light* dirLight = static_cast<Light*>(scene_->GetDirectionalLight());
		dirLight->UpdateLightParameters();

		SetGIParameters();

		//abort variant matrices
		defaultPass_->SetShaderParameter("scene_dim_max",XMFLOAT4(GetMaxBb().x,GetMaxBb().y,GetMaxBb().z,1.0f));
		defaultPass_->SetShaderParameter("scene_dim_min",XMFLOAT4(GetMinBb().x,GetMinBb().y,GetMinBb().z,1.0f));
	}

	void YumeMiscRenderer::ApplyShaderParameters(RenderCall* call)
	{
		const YumeMap<YumeHash,Variant>::type& variants = call->GetShaderVariants();
		YumeMap<YumeHash,Variant>::const_iterator It = variants.begin();

		for(It; It != variants.end(); ++It)
		{
			rhi_->SetShaderParameter(It->first,It->second);
		}

		////Matrices
		const YumeMap<YumeHash,DirectX::XMMATRIX>::type& matrices = call->GetShaderMatrices();
		YumeMap<YumeHash,DirectX::XMMATRIX>::const_iterator mIt = matrices.begin();

		for(mIt; mIt != matrices.end(); ++mIt)
		{
			rhi_->SetShaderParameter(mIt->first,mIt->second);
		}

		const YumeMap<YumeHash,DirectX::XMFLOAT4>::type& vectors = call->GetShaderVectors4();
		YumeMap<YumeHash,DirectX::XMFLOAT4>::const_iterator vIt = vectors.begin();

		for(vIt; vIt != vectors.end(); ++vIt)
		{
			rhi_->SetShaderParameter(vIt->first,vIt->second);
		}

		const YumeMap<YumeHash,DirectX::XMFLOAT3>::type& vectors3 = call->GetShaderVectors3();
		YumeMap<YumeHash,DirectX::XMFLOAT3>::const_iterator vIt3 = vectors3.begin();

		for(vIt3; vIt3 != vectors3.end(); ++vIt3)
		{
			rhi_->SetShaderParameter(vIt3->first,vIt3->second);
		}
	}

	void YumeMiscRenderer::Render()
	{
		if(!defaultPass_->calls_.size())
		{
			rhi_->BindBackbuffer();
			rhi_->Clear(CLEAR_COLOR);
		}
		else
		{
			unsigned callSize = defaultPass_->calls_.size();

			for(int i=0; i < callSize; ++i)
			{
				RenderCallPtr call = defaultPass_->calls_[i];

				if(!call->GetEnabled())
					continue;


				ApplyRendererFlags(call);


				switch(call->GetType())
				{
				case CallType::CLEAR:
				{
					RHIEvent e("RenderCall::Clear");
					for(int j=0; j < call->GetNumInputs(); ++j)
					{
						Texture2DPtr input = call->GetInput(j);

						if(input)
						{
							gYume->pRHI->SetRenderTarget(j,input);

							gYume->pRHI->ClearRenderTarget(j,call->GetClearFlags(),call->GetClearColor(j));
						}
					}

					if(call->GetDepthStencil())
					{
						gYume->pRHI->SetDepthStencil(call->GetDepthStencil());
						rhi_->ClearDepthStencil(CLEAR_DEPTH,1.0f,0);
					}


				}
				break;
				case CallType::SCENE:
				{
					RHIEvent ev;
					if(call->GetPassName().length())
					{
						ev.BeginEvent(call->GetPassName());
					}
					else
						ev.BeginEvent("RenderCall::Scene");

					if(call->IsShadowPass())
					{
						if(GetGIEnabled())
						{
							RenderReflectiveShadowMap(call);
						}
						else
						{

						}
					}
					else
					{
						bool deferred = true;

						if(deferred)
						{
							Texture2DPtr colors = call->GetOutput(0);
							Texture2DPtr normals = call->GetOutput(1);
							Texture2DPtr lineardepth = call->GetOutput(2);
							Texture2DPtr spec = call->GetOutput(3);

							rhi_->SetRenderTarget(0,colors);
							rhi_->SetRenderTarget(1,normals);
							rhi_->SetRenderTarget(2,lineardepth);
							rhi_->SetRenderTarget(3,spec);

							rhi_->BindDefaultDepthStencil();

							rhi_->SetViewport(IntRect(0,0,rhi_->GetWidth(),rhi_->GetHeight()));

							rhi_->ClearDepthStencil(CLEAR_DEPTH,1.0f,0.0f);
							rhi_->ClearRenderTarget(0,CLEAR_COLOR | CLEAR_DEPTH,YumeColor(0,0,0,1));
							rhi_->ClearRenderTarget(1,CLEAR_COLOR | CLEAR_DEPTH,YumeColor(0,0,0,1));
							rhi_->ClearRenderTarget(2,CLEAR_COLOR | CLEAR_DEPTH,YumeColor(0,0,0,1));
							rhi_->ClearRenderTarget(3,CLEAR_COLOR | CLEAR_DEPTH,YumeColor(0,0,0,1));

							rhi_->SetBlendMode(BLEND_REPLACE);

							rhi_->SetShaders(call->GetVs(),call->GetPs());

							SetSamplers(call);

							//rhi_->BindSampler(PS,0,1,0); //0 is standard filter
							SetCameraParameters(false);
							RenderScene();
							rhi_->BindResetRenderTargets(4);
						}
					}
					ev.EndEvent();
				}
				break;
				case CallType::LPV_INJECT:
				{

					if(!updateRsm_)
						break;

					RHIEvent ev;
					if(call->GetPassName().length())
					{
						ev.BeginEvent(call->GetPassName());
					}
					else
						ev.BeginEvent("RenderCall::LPV_INJECT");

					rhi_->SetViewport(IntRect(0,0,32,32));

					rhi_->SetBlendMode(BLEND_ADD);

					SetSamplers(call);

					rhi_->SetShaders(call->GetVs(),call->GetPs(),call->GetGs());

					DirectX::XMMATRIX I = DirectX::XMMatrixIdentity();
					DirectX::XMFLOAT4X4 i;
					DirectX::XMStoreFloat4x4(&i,I);
					SetModelMatrix(i,bbMin,bbMax);

					ApplyShaderParameters(call);

					//RenderCall* rsmRender = defaultPass_->GetRenderCall(1); //1 is RSM renderer

					//Texture2DPtr rsmColors = rsmRender->GetOutput(0);
					//Texture2DPtr rsmNormals = rsmRender->GetOutput(1);
					//Texture2DPtr rsmLinearDepth = rsmRender->GetOutput(2);

					//Texture2DPtr textures[3] ={rsmLinearDepth,rsmColors,rsmNormals};
					//rhi_->VSBindSRV(6,3,textures);

					unsigned numOutputs = call->GetNumOutputs();

					for(int i=0; i < MAX_RENDERTARGETS; ++i)
					{
						Texture2DPtr output = call->GetOutput(i);

						if(output)
						{
							rhi_->SetRenderTarget(i,output);
						}
					}

					unsigned inputSize = call->GetNumInputs();
					YumeVector<Texture2DPtr>::type inputs;

					unsigned startIndex = 0;

					for(unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
					{
						Texture2DPtr input = call->GetInput(i);

						if(input)
						{
							if(startIndex == 0) startIndex = i;
							inputs.push_back(input);
						}
					}
					rhi_->VSBindSRV(startIndex,inputSize,&inputs[0]);

					gYume->pRHI->BindPsuedoBuffer();

					unsigned int num_vpls = static_cast<unsigned int>(1024*1024);

					rhi_->Draw(POINT_LIST,0,num_vpls);
					gYume->pRHI->BindResetRenderTargets(4);
					gYume->pRHI->BindResetTextures(startIndex,inputSize); //Start at 6 count 3

					ev.EndEvent();
				}
				break;
				case LPV_NORMALIZE:
				{
					if(!updateRsm_)
						break;

					RHIEvent ev;
					if(call->GetPassName().length())
					{
						ev.BeginEvent(call->GetPassName());
					}
					else
						ev.BeginEvent("RenderCall::LPV_NORMALIZE");



					std::swap(curr_,next_);
					rhi_->SetViewport(IntRect(0,0,32,32));

					unsigned numOutputs = call->GetNumOutputs();

					for(int i=0; i < MAX_RENDERTARGETS; ++i)
					{
						Texture2DPtr output = call->GetOutput(i);

						if(output)
						{
							rhi_->SetRenderTarget(i,output);
						}
					}

					unsigned inputSize = call->GetNumInputs();
					YumeVector<Texture2DPtr>::type inputs;

					unsigned startIndex = 0;

					for(unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
					{
						Texture2DPtr input = call->GetInput(i);

						if(input)
						{
							if(startIndex == 0) startIndex = i;
							inputs.push_back(input);
						}
					}
					rhi_->PSBindSRV(startIndex,inputSize,&inputs[0]);

					rhi_->SetBlendMode(BLEND_ADD);

					rhi_->SetShaders(call->GetVs(),call->GetPs(),call->GetGs());

					SetGIParameters();



					static YumeVector<YumeVertexBuffer*>::type vertexBuffers(1);
					static YumeVector<unsigned>::type elementMasks(1);
					vertexBuffers[0] = lightPropagator_.GetLPVVolume()->GetVertexBuffer(0);
					elementMasks[0] = lightPropagator_.GetLPVVolume()->GetVertexBuffer(0)->GetElementMask();
					rhi_->SetVertexBuffers(vertexBuffers,elementMasks);

					rhi_->BindNullIndexBuffer();

					rhi_->Draw(TRIANGLE_LIST,0,6 * 32);

					rhi_->BindResetRenderTargets(3);
					rhi_->BindResetTextures(7,4,true);
				}
				break;
				case LPV_PROPAGATE:
				{
					if(!updateRsm_)
						break;

					RHIEvent ev;
					if(call->GetPassName().length())
					{
						ev.BeginEvent(call->GetPassName());
					}
					else
						ev.BeginEvent("RenderCall::LPV_PROPAGATE");

					unsigned numOutputs = call->GetNumOutputs();

					for(int i=0; i < MAX_RENDERTARGETS; ++i)
					{
						Texture2DPtr output = call->GetOutput(i);

						if(output)
						{
							rhi_->SetRenderTarget(i,output);
						}
					}

					unsigned inputSize = call->GetNumInputs();
					YumeVector<Texture2DPtr>::type inputs;

					unsigned startIndex = 0;

					for(unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
					{
						Texture2DPtr input = call->GetInput(i);

						if(input)
						{
							if(startIndex == 0) startIndex = i;
							inputs.push_back(input);
						}
					}
					rhi_->PSBindSRV(startIndex,inputSize,&inputs[0]);


					rhi_->SetBlendMode(BLEND_ADD);

					rhi_->SetShaders(call->GetVs(),call->GetPs(),call->GetGs());

					for(int i=0; i < 64; ++i)
					{
						rhi_->SetShaderParameter("iteration",(float)i);
						LPVPropagate(call,i);
					}
#ifndef DEBUGGING_RSM
					updateRsm_ = false;
					defaultPass_->DisableRenderCalls("LPVDeferred");
#endif

				}
				break;
				case CallType::GENERATEMIPS:
				{
					RHIEvent ev;
					if(call->GetPassName().length())
					{
						ev.BeginEvent(call->GetPassName());
					}
					else
						ev.BeginEvent("RenderCall::GENERATEMIPS");

					for(int i=0; i < call->GetNumInputs(); ++i)
					{
						Texture2DPtr input = call->GetInput(i);
						if(input)
							rhi_->GenerateMips(input);
					}
				}
				break;
				case CallType::FSTRIANGLE:
				{
					rhi_->SetViewport(IntRect(0,0,rhi_->GetWidth(),rhi_->GetHeight()));

					SetSamplers(call);

					YumeShaderVariation* fsTriangle = rhi_->GetShader(VS,"LPV/fs_triangle");

					rhi_->SetShaders(fsTriangle,call->GetPs());

					unsigned startIndex = 0;
					unsigned endIndex = 0;
					YumeVector<Texture2DPtr>::type inputs;

					for(int i=0; i < call->GetNumInputs(); ++i)
					{
						Texture2DPtr input = call->GetInput(i);

						inputs.push_back(input);
					}

					rhi_->PSBindSRV(2,call->GetNumInputs(),&inputs[0]);

					DirectX::XMMATRIX I = DirectX::XMMatrixIdentity();
					DirectX::XMFLOAT4X4 i;
					DirectX::XMStoreFloat4x4(&i,I);
					SetModelMatrix(i,bbMin,bbMax);

					//Variants
					ApplyShaderParameters(call);


					if(GetGIEnabled())
					{
						Light* light = static_cast<Light*>(scene_->GetDirectionalLight());

						const DirectX::XMFLOAT4& pos = light->GetPosition();
						const DirectX::XMFLOAT4& dir = light->GetDirection();
						const YumeColor& color = light->GetColor();

						const unsigned fSize = 4 * 3 * 4;
						float f[fSize] ={
							pos.x,pos.y,pos.z,pos.w,
							dir.x,dir.y,dir.z,dir.w,
							color.r_,color.g_,color.b_,color.a_
						};

						gYume->pRHI->SetShaderParameter("main_light",f,4*3);
					}

					SetCameraParameters(false);

					rhi_->BindResetRenderTargets(6);
					rhi_->SetDepthStencil((YumeTexture2D*)0);

					Texture2DPtr output = call->GetOutput(0);
					if(!output)
					{
						rhi_->BindBackbuffer();
						rhi_->Clear(CLEAR_COLOR);
					}
					else
					{
						rhi_->SetRenderTarget(0,output);
						rhi_->ClearRenderTarget(0,CLEAR_COLOR);
					}

					GetFsTriangle()->Draw(gYume->pRHI);

					rhi_->BindResetRenderTargets(1);
					rhi_->BindResetTextures(0,13);

				}
				break;
				default:
					break;
				}
			}
		}

	}

	void YumeMiscRenderer::RenderReflectiveShadowMap(RenderCall* call)
	{
		if(GetGIEnabled() && call->IsShadowPass() && updateRsm_)
		{
			//Set RSM Render targets
			//Already set

			//Set renderer opts
			rhi_->SetDepthTest(CMP_ALWAYS);
			rhi_->SetColorWrite(true);
			rhi_->SetFillMode(FILL_SOLID);
			rhi_->SetClipPlane(false);
			rhi_->SetScissorTest(false);

			//Clear render targets

			//Already cleared
			rhi_->SetViewport(IntRect(0,0,rsmSize,rsmSize));

			//
			SetSamplers(call);

			rhi_->SetDepthTest(CMP_LESS);
			rhi_->SetStencilTest(true,CMP_ALWAYS,OP_KEEP,OP_KEEP,OP_INCR,OP_DECR);

			//Set shaders
			rhi_->SetShaders(call->GetVs(),call->GetPs(),0);

			SetCameraParameters(true);
			RenderScene();

			rhi_->BindResetRenderTargets(4);
		}
	}

	void YumeMiscRenderer::SetSamplers(RenderCall* call)
	{
		if(call->HasVertexSampler())
		{
			unsigned num = call->GetNumVertexSamplers();

			YumeVector<unsigned>::type vsSamplers;
			for(int k=0; k < num; ++k)
			{
				unsigned samplerId = call->GetVertexSampler(k);
				vsSamplers.push_back(samplerId);
			}
			rhi_->BindSampler(VS,0,num,&vsSamplers[0]);
		}


		if(call->HasPixelSampler())
		{
			unsigned num = call->GetNumPixelSamplers();
			YumeVector<unsigned>::type psSamplers;
			for(int k=0; k < num; ++k)
			{
				unsigned samplerId = call->GetPixelSampler(k);
				psSamplers.push_back(samplerId);
			}
			rhi_->BindSampler(PS,0,num,&psSamplers[0]);
		}
	}


	void YumeMiscRenderer::LPVPropagate(RenderCall* call,float iteration)
	{
		//LPV_R_0 ID 0
		//LPV_G_0 ID 1
		//LPV_B_0 ID 2
		//LPV_R_1 ID 3
		//LPV_G_1 ID 4
		//LPV_B_1 ID 5
		//LPV_ACCUMR ID 6
		//LPV_ACCUMG ID 7
		//LPV_ACCUMB ID 8
		//LPV_INJECT_COUNTER ID 9
		std::swap(curr_,next_);

		YumeString eventName = "Iteration ";
		eventName.AppendWithFormat("%f",iteration);

		RHIEvent e(eventName,true);

		Texture2DPtr lpvr_next = defaultPass_->GetTextureByName(GetTextureName("LPV_R_",next_));
		Texture2DPtr lpvg_next = defaultPass_->GetTextureByName(GetTextureName("LPV_G_",next_));
		Texture2DPtr lpvb_next = defaultPass_->GetTextureByName(GetTextureName("LPV_B_",next_));

		Texture2DPtr lpvr_curr = defaultPass_->GetTextureByName(GetTextureName("LPV_R_",curr_));
		Texture2DPtr lpvg_curr = defaultPass_->GetTextureByName(GetTextureName("LPV_G_",curr_));
		Texture2DPtr lpvb_curr = defaultPass_->GetTextureByName(GetTextureName("LPV_B_",curr_));

		Texture2DPtr lpvaccumr = call->GetOutput(3);
		Texture2DPtr lpvaccumg = call->GetOutput(4);
		Texture2DPtr lpvaccumb = call->GetOutput(5);


		rhi_->SetRenderTarget(0,lpvr_next);
		rhi_->SetRenderTarget(1,lpvg_next);
		rhi_->SetRenderTarget(2,lpvb_next);
		rhi_->SetRenderTarget(3,lpvaccumr);
		rhi_->SetRenderTarget(4,lpvaccumg);
		rhi_->SetRenderTarget(5,lpvaccumb);

		rhi_->ClearRenderTarget(0,CLEAR_COLOR);
		rhi_->ClearRenderTarget(1,CLEAR_COLOR);
		rhi_->ClearRenderTarget(2,CLEAR_COLOR);

		YumeTexture2D* textures[4] ={lpvr_curr,lpvg_curr,lpvb_curr};
		gYume->pRHI->PSBindSRV(7,3,textures);

		SetGIParameters();

		ApplyShaderParameters(call);

		rhi_->BindNullIndexBuffer();

		lightPropagator_.GetLPVVolume()->Draw(gYume->pRHI);

		rhi_->BindResetRenderTargets(6);
		rhi_->BindResetTextures(7,3,true);

		
	}

	YumeString YumeMiscRenderer::GetTextureName(const YumeString& s,int num)
	{
		YumeString ss = s;
		ss.AppendWithFormat("%i",num);
		return ss;
	}

	void YumeMiscRenderer::ApplyRendererFlags(RenderCallPtr call)
	{
		unsigned flags = call->GetRendererFlags();

		YumeRHI* rhi = gYume->pRHI;

		//Defaults
		rhi->SetNoDepthStencil(false);
		rhi->SetNoBlendState(false);

		if(flags & RF_NODEPTHSTENCIL)
			rhi->SetNoDepthStencil(true);

		if(flags & RF_NOBLENDSTATE)
			rhi->SetNoBlendState(true);
	}

	void YumeMiscRenderer::SetGIParameters()
	{
		defaultPass_->SetShaderParameter("gi_scale",giParams_.Scale);
		defaultPass_->SetShaderParameter("lpv_flux_amplifier",giParams_.LPVFlux);
		defaultPass_->SetShaderParameter("debug_gi",giParams_.DebugView);
	}

	void YumeMiscRenderer::SetModelMatrix(const DirectX::XMFLOAT4X4& model,const DirectX::XMFLOAT3& lpvMin,const DirectX::XMFLOAT3& lpvMax)
	{
		DirectX::XMMATRIX model_inv = DirectX::XMMatrixInverse(nullptr,DirectX::XMLoadFloat4x4(&model));

		DirectX::XMVECTOR diag = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&lpvMax),DirectX::XMLoadFloat3(&lpvMin));

		DirectX::XMFLOAT3 d;
		DirectX::XMStoreFloat3(&d,diag);

		DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(1.f/d.x,1.f/d.y,1.f/d.z);

		DirectX::XMMATRIX trans = DirectX::XMMatrixTranslation(-lpvMin.x,-lpvMin.y,-lpvMin.z);

		DirectX::XMMATRIX world_to_lpv = model_inv * trans * scale;

		gYume->pRHI->SetShaderParameter("world_to_lpv",world_to_lpv);
		gYume->pRHI->SetShaderParameter("lpv_size",(float)32.0f);
	}

	void YumeMiscRenderer::RenderFullScreenTexture(const IntRect& rect,YumeTexture2D* overlaytexture)
	{
		YumeTexture2D* texture[] ={overlaytexture};
		rhi_->PSBindSRV(10,1,texture);



		YumeShaderVariation* triangle = gYume->pRHI->GetShader(VS,"LPV/fs_triangle");

		rhi_->SetBlendMode(BLEND_PREMULALPHA);
		rhi_->BindSampler(PS,0,1,0); //Standard
		rhi_->SetShaders(triangle,overlayPs_,0);

		rhi_->BindBackbuffer();

		fullscreenTriangle_->Draw(rhi_);
	}

	void YumeMiscRenderer::RenderSky()
	{
		//Sky will set its shaders.

	}

	void YumeMiscRenderer::RenderScene()
	{
		SceneNodes::type& renderables = scene_->GetRenderables();

		for(int i=0; i < renderables.size(); ++i)
		{
			SceneNode* node = renderables[i];

			YumeMesh* geometry = node->GetGeometry();

			geometry->Render();
		}
	}

	void YumeMiscRenderer::SetGIDebug(bool enabled)
	{
		lpv_->SetGIDebug(enabled);
	}

	void YumeMiscRenderer::SetGIScale(float f)
	{
		lpv_->SetGIScale(f);
	}

	void YumeMiscRenderer::SetLPVFluxAmp(float f)
	{
		lpv_->SetLPVFlux(f);
	}

	void YumeMiscRenderer::SetLPVPos(float x,float y,float z)
	{
		lpv_->SetLPVPos(x,y,z);
	}

	void YumeMiscRenderer::SetLightFlux(float f)
	{
		lpv_->SetLightFlux(f);
	}

	void YumeMiscRenderer::SetLPVNumberIterations(int num)
	{
		lpv_->SetNumIterations(num);
	}

	void YumeMiscRenderer::SetPerFrameConstants()
	{
		float totalTime = gYume->pTimer->GetElapsedTime();
		float timeStep = gYume->pTimer->GetTimeStep();

		rhi_->SetShaderParameter("time_delta",timeStep);
		rhi_->SetShaderParameter("time",totalTime);
	}

	void YumeMiscRenderer::UpdateMeshBb(YumeMesh& mesh)
	{
		DirectX::XMVECTOR v_bb_min = XMLoadFloat3(&mesh.bb_min());
		DirectX::XMVECTOR v_bb_max = XMLoadFloat3(&mesh.bb_max());

		DirectX::XMVECTOR v_diag = DirectX::XMVectorSubtract(v_bb_max,v_bb_min);

		DirectX::XMVECTOR delta = DirectX::XMVectorScale(v_diag,0.05f);

		v_bb_min = DirectX::XMVectorSubtract(v_bb_min,delta);
		v_bb_max = DirectX::XMVectorAdd(v_bb_max,delta);

		v_bb_min = DirectX::XMVectorSetW(v_bb_min,1.f);
		v_bb_max = DirectX::XMVectorSetW(v_bb_max,1.f);

		auto world = DirectX::XMLoadFloat4x4(&mesh.world());
		v_bb_min = DirectX::XMVector4Transform(v_bb_min,world);
		v_bb_max = DirectX::XMVector4Transform(v_bb_max,world);

		DirectX::XMStoreFloat3(&bbMin,v_bb_min);
		DirectX::XMStoreFloat3(&bbMax,v_bb_max);
	}

	void YumeMiscRenderer::SetCameraParameters(bool shadowPass)
	{
		XMMATRIX view = GetCamera()->GetViewMatrix();
		XMMATRIX proj = GetCamera()->GetProjMatrix();
		XMFLOAT4 cameraPos;

		SceneNode* dirLight = scene_->GetDirectionalLight();

		if(!dirLight)
			return;

		if(shadowPass)
		{
			XMStoreFloat4(&cameraPos,XMLoadFloat4(&dirLight->GetPosition()));
			XMMATRIX lightView = XMMatrixLookToLH(XMLoadFloat4(&dirLight->GetPosition()),XMLoadFloat4(&dirLight->GetDirection()),XMLoadFloat4(&dirLight->GetRotation()));

			view = lightView;
			proj = MakeProjection();
		}
		else
			XMStoreFloat4(&cameraPos,GetCamera()->GetEyePt());

		XMMATRIX vp = view * proj;
		XMMATRIX vpInv = XMMatrixInverse(nullptr,vp);

		gYume->pRHI->SetShaderParameter("vp",vp);
		gYume->pRHI->SetShaderParameter("vp_inv",vpInv);
		gYume->pRHI->SetShaderParameter("camera_pos",cameraPos);
		gYume->pRHI->SetShaderParameter("z_far",zFar);
	}


	DirectX::XMMATRIX YumeMiscRenderer::MakeProjection()
	{
		float n = zNear;
		float f = zFar;

		float q = f/(f-n);

		return DirectX::XMMATRIX
			(
			1.f,0.f,0.f,0.f,
			0.f,1.f,0.f,0.f,
			0.f,0.f,q,1.f,
			0.f,0.f,-q*n,0.f
			);
	}

	void YumeMiscRenderer::HandlePostRenderUpdate(float timeStep)
	{
		Update(timeStep);
	}
}
