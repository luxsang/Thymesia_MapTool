#include "pch.h" 
#include "..\Public\Terrain.h"
#include "GameInstance.h"

#include "NonMoveObject.h"
#include "EnvironmentObject.h"

CTerrain::CTerrain(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CGameObject{ pDevice, pContext }
{

}

CTerrain::CTerrain(const CTerrain& Prototype)
	: CGameObject(Prototype)
{
}

HRESULT CTerrain::Initialize_Prototype()
{
	if (FAILED(__super::Initialize_Prototype()))
		return E_FAIL;

	return S_OK;
}

HRESULT CTerrain::Initialize(void* pArg)
{
	TERRAINDESC* pDesc = static_cast<TERRAINDESC*>(pArg);

	if (pDesc->pMaskTexture != nullptr)
		m_pMaskTexture = pDesc->pMaskTexture;

	if (pDesc->pWaterMapTexture != nullptr)
		m_pWaterMapTexture = pDesc->pWaterMapTexture;

	if (FAILED(__super::Initialize(nullptr)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
		return E_FAIL;

	m_pTransformCom->Set_State(CTransform::STATE_POSITION, XMVectorSet(30.0f, 0.0f, -150.0f, 1.0f));

	return S_OK;
}

void CTerrain::Priority_Update(_float fTimeDelta)
{
	//m_pNavigationCom->SetUp_WorldMatrix(XMLoadFloat4x4(m_pTransformCom->Get_WorldMatrix_Ptr()));
}

void CTerrain::Update(_float fTimeDelta)
{
	int a = 10;

	ImGui::Begin("Switch FrameMode", NULL, ImGuiWindowFlags_MenuBar);
	ImGui::Checkbox("Switching To WireFrame", &m_bWireFrameMode);
	if (ImGui::Button("WireFrame On"))
		m_bWireFrameMode = true;
	if (ImGui::Button("WireFrame Off"))
		m_bWireFrameMode = false;

	ImGui::InputText("JsonPath", FilePath, IM_ARRAYSIZE(FilePath));
	if (ImGui::Button("Get_Mesh_Data"))
	{
		m_pJsonExtractor->FindJSonPathes(FilePath);
	}

	ImGui::End();

	if (m_pGameInstance->isKeyPressed(DIK_K))//txt에 메쉬 이름들 저장
	{
		m_pJsonExtractor->Export_MeshName("../Bin/DataFiles/MeshNames/Mesh_Name_0.txt");
	}
	if (m_pGameInstance->isKeyPressed(DIK_I))//txt에 메쉬 경로들 저장
	{
		m_pJsonExtractor->Export_MeshPath("../Bin/DataFiles/MeshPath/Mesh_Path_0.txt");
	}
}

void CTerrain::Late_Update(_float fTimeDelta)
{
	m_pGameInstance->Add_RenderGroup(CRenderer::RG_NONBLEND, this);
}

HRESULT CTerrain::Render()
{
	/* 네비이게이션 버퍼 확인용으로 잠깐 뺌*/
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	m_bWireFrameMode ? m_pShaderCom->Begin(1) : m_pShaderCom->Begin(2);

	//m_pShaderCom->Begin(0);

	m_pVIBufferCom->Bind_InputAssembler();

	//CTempManager::GetInstance()->SwitchFrameMode();

	m_pVIBufferCom->Render();

#ifdef _DEBUG				
	m_pNavigationCom->Render();
#endif // _DEBUG


	return S_OK;
}

void CTerrain::Set_TerrainPickPos(_float3 _fPickPos, _float _fRange)
{
	m_fPickPos = _fPickPos;
	m_fRange = _fRange;
}

HRESULT CTerrain::Make_Meshes(list<CObject*>& Objects,
	vector<CEnvironmentObject*>& EnvironmentObjects,
	map<string, vector<CObject*>>& mapStaticObjects,
	map<string, CEnvironmentObject*>& mapInstanceObjects, vector<string>& StaticObjectsNames, vector<string>& InstanceObjectsNames) // 메쉬 생성
{
	vector< MESHINFO_DESC> MeshInfo;

	m_pJsonExtractor->Get_Infos(MeshInfo);

	if (MeshInfo.size() != 0) //받아온 메쉬정보가 없으면 continue
	{
		vector<MESHINFO_DESC> MeshNames;


		vector<string> fileNames;

		for (auto& pMeshes : MeshInfo)
		{
			fileNames.push_back(pMeshes.ObjectName); //메쉬이름 저장
		}

		std::sort(fileNames.begin(), fileNames.end());
		fileNames.erase(std::unique(fileNames.begin(), fileNames.end()), fileNames.end()); // 메쉬 이름 겹치는거 삭제 

		map <string, vector<CNonMoveObject::OBJECT_DESC>> mapMeshInstances; // 메쉬 정보를 담을 map
		map <string, _uint> mapMeshSize; // 각 메쉬 갯수를 담을 map

		for (auto& Name : fileNames)
		{
			vector <CNonMoveObject::OBJECT_DESC> Desc;
			mapMeshSize.insert({ Name, 0 });
			mapMeshInstances.insert({ Name,  Desc }); // map 초기화 과정
		}

		for (auto& pMesh : MeshInfo)
		{
			CNonMoveObject::OBJECT_DESC Desc = {};

			Desc.fFrustumRadius = { 1.f };
			Desc.fPosition = _float4(pMesh.fPosition.x, pMesh.fPosition.y, pMesh.fPosition.z, 1.f);

			if (XMVector3Equal(XMVectorSet(0.f, 0.f, 0.f, 1.f), XMLoadFloat4(&Desc.fPosition)))
			{
				_vector vIdentity = XMVectorSet(0.0001f, 0.0001f, 0.00001f, 1.f); // position이 완전히 0이 되지 않게 해줌 ( posiiton이 0인 값을 transform생성시 무시함)

				XMStoreFloat4(&Desc.fPosition, vIdentity);
			}
			Desc.fRotation = pMesh.fRotation;
			Desc.fScaling = pMesh.fScale;
			Desc.ObjectName = pMesh.ObjectName;

			mapMeshInstances[Desc.ObjectName].push_back(Desc);
			mapMeshSize[Desc.ObjectName]++;

		}

		for (auto& MeshName : fileNames)
		{
			if (mapMeshSize[MeshName] > 8) // 메쉬의 갯수가 8개 이상일 경우 인스턴스 오브젝트 생성
			{
				vector<_int> instanceBox;

				_int iMeshSize = mapMeshSize[MeshName];

				vector<VTX_MODEL_INSTANCE> vecInstanceData; // 메쉬 갯수만큼 늘려주기
				vector<_float3> vecInstancePosition;
				vector<_float3> vecInstanceScale;
				vector<_float4> vecInstanceRotation;

				_int vectorBoxSize = {}; // 박스 갯수

				_uint iInstanceDataSize = { 0 };
				for (auto& pMeshInfo : mapMeshInstances[MeshName])
				{
					_float4 fRotation = pMeshInfo.fRotation;
					_vector vQuaternion = XMLoadFloat4(&fRotation); // 회전
					_matrix matRotation = XMMatrixRotationQuaternion(vQuaternion);

					_float3 fPosition = _float3(pMeshInfo.fPosition.x, pMeshInfo.fPosition.y, pMeshInfo.fPosition.z);
					_vector vPosition = XMLoadFloat3(&fPosition); // 위치
					_matrix matPosition = XMMatrixTranslation(XMVectorGetX(vPosition), XMVectorGetY(vPosition), XMVectorGetZ(vPosition));

					_float3 fScale = pMeshInfo.fScaling;
					_vector vScale = XMLoadFloat3(&fScale); // 크기
					_matrix matScale = XMMatrixScaling(XMVectorGetX(vScale), XMVectorGetY(vScale), XMVectorGetZ(vScale));

					_matrix matWorld = matScale * matRotation * matPosition; // 순서 조심 (Scale * Rotation * Position)

					VTX_MODEL_INSTANCE Instance = {};

					for (size_t i = 0; i < 4; ++i)
					{
						XMStoreFloat4(&Instance.InstanceMatrix[i], matWorld.r[i]);
					}

					vectorBoxSize++;
					vecInstanceData.push_back(Instance);

					vecInstancePosition.push_back(fPosition);
					vecInstanceRotation.push_back(fRotation);
					vecInstanceScale.push_back(fScale);
				}

				CEnvironmentObject::ENVIRONMENT_OBJECT_DESC EnvironmentDesc = {};
				EnvironmentDesc.fPosition = { 0.01f, 0.01f, 0.01f, 1.f };
				EnvironmentDesc.fFrustumRadius = 1.f;

				EnvironmentDesc.fScaling = { 0.01f, 0.01f, 0.01f };
				EnvironmentDesc.fRotation = { 0.0f, 0.1f, 0.0f, 1.0f };

				EnvironmentDesc.iPassNum = { 0 };
				EnvironmentDesc.ObjectName = MeshName;


				instanceBox.push_back(1);

				EnvironmentDesc.fSpace = 1.f; // 원 크기?
				EnvironmentDesc.isBasicMode = false; // true 면 instance갯수만큼 생성 아닐 시 원 크기만큼 생성
				EnvironmentDesc.vecBoxSize = instanceBox;


				EnvironmentDesc.bCullingObject = true;
				EnvironmentDesc.fInstanceCount = iMeshSize;

				EnvironmentDesc.vecInstancePosition = vecInstancePosition;
				EnvironmentDesc.vecInstanceScale = vecInstanceScale;
				EnvironmentDesc.vecInstanceRotation = vecInstanceRotation;

				CEnvironmentObject* pEnvironment = reinterpret_cast<CEnvironmentObject*>(m_pGameInstance->Add_GameObject_To_Layer_Take(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Object_GroundObject"),
					LEVEL_GAMEPLAY, TEXT("Layer_GroundObject"), &EnvironmentDesc));

				if (pEnvironment != nullptr)
				{
					EnvironmentObjects.push_back(pEnvironment);
					mapInstanceObjects.insert({ MeshName, pEnvironment });
					InstanceObjectsNames.push_back(MeshName);
				}
			}
			else //아닐 경우 일반 static object 생성
			{
				vector<CObject*> vecObject;
				mapStaticObjects.insert({ MeshName, vecObject });

				for (auto& iter : mapMeshInstances[MeshName])
				{
					CObject* pObject = reinterpret_cast<CObject*>(m_pGameInstance->Add_GameObject_To_Layer_Take(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Object_NonMoveObject"), LEVEL_GAMEPLAY, TEXT("Layer_Object"), &iter));

					if (pObject != nullptr)
					{
						Objects.push_back(pObject);
						mapStaticObjects[MeshName].push_back(pObject);
						StaticObjectsNames.push_back(MeshName);
					}
				}
			}
		}
	}


	std::sort(StaticObjectsNames.begin(), StaticObjectsNames.end());
	StaticObjectsNames.erase(std::unique(StaticObjectsNames.begin(), StaticObjectsNames.end()), StaticObjectsNames.end());

	std::sort(InstanceObjectsNames.begin(), InstanceObjectsNames.end());
	InstanceObjectsNames.erase(std::unique(InstanceObjectsNames.begin(), InstanceObjectsNames.end()), InstanceObjectsNames.end());
	return S_OK;
}

HRESULT CTerrain::Ready_Components()
{
	/* Com_Texture */
	if (FAILED(__super::Add_Component(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Texture_Terrain"),
		TEXT("Com_Texture"), reinterpret_cast<CComponent**>(&m_pTextureCom))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Texture_Terrain_Normal"),
		TEXT("Com_NormalTexture"), reinterpret_cast<CComponent**>(&m_pTextureNormalCom))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Texture_Terrain_ORM"),
		TEXT("Com_ORMTexture"), reinterpret_cast<CComponent**>(&m_pTextureORMCom))))
		return E_FAIL;

	// Prototype_Component_Texture__MouseRange
	if (FAILED(__super::Add_Component(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Texture__MouseRange"),
		TEXT("Com_MouseTexture"), reinterpret_cast<CComponent**>(&m_pMouseTextureCom))))
		return E_FAIL;

	/* Com_Shader */
	if (FAILED(__super::Add_Component(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Shader_VtxNorTex"),
		TEXT("Com_Shader"), reinterpret_cast<CComponent**>(&m_pShaderCom))))
		return E_FAIL;

	/* Com_VIBuffer */
	if (FAILED(__super::Add_Component(LEVEL_GAMEPLAY, TEXT("Prototype_Component_VIBuffer_Terrain"),
		TEXT("Com_VIBuffer_Terrain"), reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
		return E_FAIL;

	/* Com_Navigation */
	if (FAILED(__super::Add_Component(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Navigation"),
		TEXT("Com_Navigation"), reinterpret_cast<CComponent**>(&m_pNavigationCom))))
		return E_FAIL;

	/* Com_Extractor */
	if (FAILED(__super::Add_Component(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Json_Extractor"),
		TEXT("Com_Extractor"), reinterpret_cast<CComponent**>(&m_pJsonExtractor))))
		return E_FAIL;
	//m_pJsonExtractor

	return S_OK;
}

HRESULT CTerrain::Bind_ShaderResources()
{
	if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
		return E_FAIL;
	if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", &m_pGameInstance->Get_Transform_Float4x4(CPipeLine::D3DTS_VIEW))))
		return E_FAIL;
	if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", &m_pGameInstance->Get_Transform_Float4x4(CPipeLine::D3DTS_PROJ))))
		return E_FAIL;

	if (FAILED(m_pTextureCom->Bind_ShaderResources(m_pShaderCom, "g_DiffuseTexture")))
		return E_FAIL;

	if (FAILED(m_pTextureNormalCom->Bind_ShaderResources(m_pShaderCom, "g_NormalTexture")))
		return E_FAIL;

	if (FAILED(m_pTextureORMCom->Bind_ShaderResources(m_pShaderCom, "g_ORMTexture")))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_SRV("g_MaskTexture", m_pMaskTexture)))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_SRV("g_WaterMapTexture", m_pWaterMapTexture)))
		return E_FAIL;

	if (FAILED(m_pMouseTextureCom->Bind_ShaderResource(m_pShaderCom, "g_MouseTexture", 0)))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_BrushRange", &m_fRange, sizeof(_float))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_PickedPoints", &m_fPickPos, sizeof(_float3))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_WireFrameMode", &m_bWireFrameMode, sizeof(_bool))))
		return E_FAIL;


	return S_OK;
}

CTerrain* CTerrain::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CTerrain* pInstance = new CTerrain(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed To Created : CTerrain");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CTerrain::Clone(void* pArg)
{
	CTerrain* pInstance = new CTerrain(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed To Cloned : CTerrain");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CTerrain::Free()
{
	__super::Free();

	Safe_Release(m_pNavigationCom);
	Safe_Release(m_pShaderCom);
	Safe_Release(m_pVIBufferCom);
	Safe_Release(m_pTextureCom);
	Safe_Release(m_pMouseTextureCom);
	Safe_Release(m_pTextureNormalCom);
	Safe_Release(m_pTextureORMCom);

	Safe_Release(m_pJsonExtractor);
}
