#pragma once

#include "Client_Defines.h"
#include "GameObject.h"
#include "TempManager.h"

BEGIN(Engine)
class CShader;
class CTexture;
class CTransform;
class CVIBuffer_Terrain;
class CNavigation;
class CJsonExtractor;
END

BEGIN(Client)

class CTerrain final : public CGameObject
{
public:
	typedef struct MeshInfo {
		std::string meshPath;
		_float3 vLocation;
		_float4 vQuaternion;
		_float3 vScale;
	}MESHINFO;

public:
	typedef struct TerrainDesc : public CGameObject::GAMEOBJECT_DESC {
		ID3D11ShaderResourceView* pMaskTexture = { nullptr };
		ID3D11ShaderResourceView* pWaterMapTexture = { nullptr };

	}TERRAINDESC;

private:
	CTerrain(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CTerrain(const CTerrain& Prototype);
	virtual ~CTerrain() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual HRESULT Render() override;

public:
	void	Set_TerrainPickPos(_float3 _fPickPos, _float _fRange);
	HRESULT Make_Meshes(list<class CObject*>& Objects, vector<class CEnvironmentObject*>& EnvironmentObjects, map<string, vector<class CObject*>>& mapStaticObjects, map<string, class CEnvironmentObject*>& mapInstanceObjects
		, vector<string>& StaticObjectsNames, vector<string>& InstanceObjectsNames);
	void	Set_Brushing(_bool _bBrushing) { m_bBrushing = _bBrushing; }

private:
	CShader* m_pShaderCom = { nullptr };
	CTexture* m_pTextureCom = { nullptr };
	CTexture* m_pTextureNormalCom = { nullptr };
	CTexture* m_pTextureORMCom = { nullptr };
	CTexture* m_pMouseTextureCom = { nullptr };
	CVIBuffer_Terrain* m_pVIBufferCom = { nullptr };
	CNavigation* m_pNavigationCom = { nullptr };
	CJsonExtractor* m_pJsonExtractor = { nullptr };

private:
	_float                          m_fRange = {};
	_float3                         m_fPickPos = {};
	_bool							m_bBrushing = { false };
	_bool							m_bWireFrameMode = { false };

	ID3D11ShaderResourceView* m_pMaskTexture = { nullptr };
	ID3D11ShaderResourceView* m_pWaterMapTexture = { nullptr };

	_char FilePath[MAX_PATH] = { "../Bin/DataFiles/Jsons/MemoryOcean_Main.json" };

public:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

public:
	static CTerrain* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};

END