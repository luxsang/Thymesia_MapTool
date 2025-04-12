#pragma once

#include "Component.h"


using json = nlohmann::json;
BEGIN(Engine)

class ENGINE_DLL CJsonExtractor final : public CComponent
{
public:
	struct MeshInfo {
		std::string meshPath;
		_float3 vLocation;
		_float4 vQuaternion;
		_float3 vScale;
	};
private:
	CJsonExtractor(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CJsonExtractor(const CJsonExtractor& Prototype);
	virtual ~CJsonExtractor() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg) override;
	HRESULT FindJSonPathes(const _char* pFilePath);
	HRESULT FindMeshInfofromJson(const _char* pFilePath);
	HRESULT Export_MeshName(const _char* pFilePath);
	HRESULT Export_MeshPath(const _char* pFilePath);

	HRESULT Get_Infos(vector<MESHINFO_DESC>& pObjectInfo);

	void Find_StaticMeshes(unordered_map<int, json> mapIndex, json j);
private:
	vector<MESHINFO_DESC> results;

	unordered_map<string, json> objectMap;


public:
	static CJsonExtractor* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CComponent* Clone(void* pArg) override;
	virtual void Free() override;
};

END