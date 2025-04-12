
#include "Shader.h"
#include "..\Public\JsonExtractor.h"

//#include <filesystem>

#include "GameInstance.h"
#include <regex>

using json = nlohmann::json;

string GetFileNameFromPath(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash == std::string::npos)
        return path;
    return path.substr(lastSlash + 1);
}

std::string StripNumberSuffix(const std::string& name) {
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0 && std::isdigit(name[dot + 1])) {
        return name.substr(0, dot);
    }
    return name;
}
float SafeScale(float value) {
    return (value <= 0.00001f) ? 0.01f : value;
}
float SafeScaleToOne(float value) {
    return (value <= 0.00001f) ? 1.f : value;
}

const json* FindObjectByName(const string& name, const vector<json>& allObjects) {
    for (const auto& obj : allObjects) {
        if (obj.contains("Name") && obj["Name"] == name)
            return &obj;
    }
    return nullptr;
}

string ExtractParentName(const std::string& fullName) {
    size_t colonPos = fullName.find(':');
    if (colonPos == std::string::npos) return "";

    std::string afterColon = fullName.substr(colonPos + 1);  // 예: "PersistentLevel.BP_CircusMatrix_Planks125.DefaultSceneRoot"

    size_t firstDot = afterColon.find('.');
    if (firstDot == std::string::npos) return "";

    size_t secondDot = afterColon.find('.', firstDot + 1);
    if (secondDot == std::string::npos) return "";

    std::string actorName = afterColon.substr(firstDot + 1, secondDot - firstDot - 1);
    return actorName;
}

std::string ExtractBaseName(const std::string& input) {
    size_t start = input.find("PersistentLevel.");
    if (start == std::string::npos) return "";

    start += std::string("PersistentLevel.").length();

    size_t end = input.find('.', start);
    if (end == std::string::npos) return "";

    std::string nameWithNumber = input.substr(start, end - start);

    // 숫자 제거 (뒤에서부터)
    while (!nameWithNumber.empty() && isdigit(nameWithNumber.back())) {
        nameWithNumber.pop_back();
    }

    return nameWithNumber;
}

string ExtractQuotedText(const std::string& input) {
    size_t first = input.find('\'');
    size_t second = input.rfind('\'');

    if (first != std::string::npos && second != std::string::npos && second > first)
        return input.substr(first + 1, second - first - 1);

    return "";
}

std::string ExtractSimpleName(const std::string& fullName) {
    auto start = fullName.find('\'');
    auto end = fullName.find_last_of('\'');
    if (start != std::string::npos && end != std::string::npos && end > start) {
        return fullName.substr(start + 1, end - start - 1);
    }
    return fullName;
}


_int BuildObjectIndexMap(const json& jsonArray, unordered_map <int, json> _mapIndex) {
    unordered_map<int, nlohmann::json> indexMap;
    std::regex indexRegex(R"(\.([0-9]+)$)");

    if (!jsonArray.contains("ObjectPath"))
        return -1;

    string path = jsonArray["ObjectPath"];
    smatch match;
    if (std::regex_search(path, match, indexRegex)) {
        int index = std::stoi(match[1].str());

        const auto& prop = _mapIndex[index];
        if (prop.contains("Properties"))
        {
            const auto& childprop = prop["Properties"];

            if (childprop.contains("StaticMeshComponent"))
            {
                string childName = ExtractSimpleName(childprop["StaticMeshComponent"]["ObjectPath"]);

                smatch match2;
                if (std::regex_search(path, match2, indexRegex)) {
                    int index = std::stoi(match2[1].str());

                    return index;
                }
            }

            else if (childprop.contains("DefaultSceneRoot"))
            {
                string childName = ExtractSimpleName(childprop["DefaultSceneRoot"]["ObjectPath"]);

                smatch match2;
                if (std::regex_search(path, match2, indexRegex)) {
                    int index = std::stoi(match2[1].str());

                    return index;
                }
            }
        }
        else
            return -1;
    }

    return -1;
}

string ExtractVariableName(string strInput)
{
    size_t colonPos = strInput.find(':');
    if (colonPos != std::string::npos) {
        size_t endQuotePos = strInput.find('\'', colonPos);
        if (endQuotePos != std::string::npos) {
            string result = strInput.substr(colonPos + 1, endQuotePos - colonPos - 1);
            return result;
        }
    }

    return "";
}

CJsonExtractor::CJsonExtractor(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CComponent{ pDevice, pContext }
{
}

CJsonExtractor::CJsonExtractor(const CJsonExtractor& Prototype)
	: CComponent(Prototype)
{
}

HRESULT CJsonExtractor::Initialize_Prototype()
{

	return S_OK;
}

HRESULT CJsonExtractor::Initialize(void* pArg)
{
	return S_OK;
}

inline int ExtractIndexFast(const std::string& path)
{
    size_t pos = path.rfind('.');
    if (pos != std::string::npos)
        return std::stoi(path.substr(pos + 1));
    return -1;
}

HRESULT CJsonExtractor::FindJSonPathes(const _char* pFilePath) // 메쉬 경로와 Transform가져오는 함수
{
    std::ifstream file(pFilePath);
    if (!file.is_open()) {
        std::cerr << "파일을 열 수 없습니다.\n";
        return E_FAIL;
    }
    json j;
    file >> j;

    unordered_map <int, json> mapIndex;

    _uint imapIndex = { 0 };

    regex indexRegex(R"(\.([0-9]+)$)"); // 인덱스 찾기용 변수들
    smatch match2;

    for (const auto& obj : j) // 모든 구조체 인덱스 화
    {
        mapIndex[imapIndex++] = obj;
    }

    for (const auto& obj : j) // Actor에 존재하는 모든 mesh찾기..(너무 오래걸림)
    {
        if (obj.contains("Type") && obj["Type"] == ("Level") && obj["Name"] == ("PersistentLevel")) // Actor에 Scene에 쓰일 오브젝트들이 모여 있음
        {
            if (obj.contains("Actors")) // Actor찾기
            {
                for (const auto& Actor : obj["Actors"]) // Actor순회 돌면서 ObjectName하나씩 Mesh맞는지 확인
                {
                    if (!Actor.contains("ObjectPath")) continue;

                    int iIndex = ExtractIndexFast(Actor["ObjectPath"]); // Path끝부분 index찾아오기에 반드시 Path로 해야함

                    const auto& iter = mapIndex[iIndex];

                    if (!iter.contains("Properties")) continue;

                    const auto& ChildProp = iter["Properties"];

                    if (ChildProp.contains("StaticMeshComponent")) // 자식이 StaticMesh 인지 InstanceMesh인지 splineMesh인지 등등.. 확인하기
                    {
                        int Childindex = ExtractIndexFast(ChildProp["StaticMeshComponent"]["ObjectPath"]);

                        const auto& ChildChildProp = mapIndex[Childindex];

                        regex indexRegex(R"(\.([0-9]+)$)"); // 인덱스 찾기용 변수들
                        smatch match2;

                        const auto& props = ChildChildProp["Properties"];
                        if (!props.contains("StaticMesh"))
                            continue;

                        MESHINFO_DESC info;
                        string strFinalPath = GetFileNameFromPath(props["StaticMesh"]["ObjectPath"]); // 경로 가져오기 (ObjectPath대신 ObjectName부르면 메쉬이름만 가져와짐)
                        strFinalPath = StripNumberSuffix(strFinalPath);
                        info.ObjectName = strFinalPath;

                        _matrix matWorld = XMMatrixIdentity();

                        if (props.contains("AttachParent")) // 부모 찾기 현재 메쉬에 부모 블루프린트 Transform에 곱하는 형식이 아니면 쓸모 X
                        {
                            string parentName = ExtractSimpleName(ChildChildProp["Properties"]["AttachParent"]["ObjectPath"]);

                            if (std::regex_search(parentName, match2, indexRegex)) { // 부모 인덱스로 찾아오기
                                int index = std::stoi(match2[1].str());

                                const auto& iter2 = mapIndex[index];

                                if (iter2.contains("Properties"))
                                {
                                    const auto& parentObject = iter2["Properties"];

                                    _vector vLocation = {};
                                    if (parentObject.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
                                        auto loc = parentObject["RelativeLocation"];
                                        vLocation.m128_f32[0] = loc["X"] * 0.01f;
                                        vLocation.m128_f32[1] = loc["Z"] * 0.01f;
                                        vLocation.m128_f32[2] = loc["Y"] * -0.01f;
                                        vLocation.m128_f32[3] = 1.f;
                                    }

                                    _vector vRotationOut = {};
                                    if (parentObject.contains("RelativeRotation")) { // 회전 따로 Z-UP to Y-UP 적용하지 않음
                                        auto rot = parentObject["RelativeRotation"];
                                        _float rotPitch = rot["Pitch"];
                                        _float rotYaw = rot["Yaw"];
                                        _float rotRoll = rot["Roll"];

                                        _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

                                        vRotationOut = vRotation;
                                    }

                                    _vector vScale = {};
                                    // 스케일
                                    if (parentObject.contains("RelativeScale3D")) { // 크기 보통 0으로 되있으므로 1로 설정
                                        auto scale = parentObject["RelativeScale3D"];
                                        _float fScaleX = scale.contains("X");
                                        _float fScaleY = scale.contains("Y");
                                        _float fScaleZ = scale.contains("Z");

                                        fScaleX = SafeScaleToOne(fScaleX);
                                        fScaleY = SafeScaleToOne(fScaleY);
                                        fScaleZ = SafeScaleToOne(fScaleZ);

                                        vScale = XMVectorSet(fScaleX, fScaleY, fScaleZ, 0.f);
                                    }
                                    vScale.m128_f32[0] = SafeScaleToOne(vScale.m128_f32[0]);
                                    vScale.m128_f32[1] = SafeScaleToOne(vScale.m128_f32[1]);
                                    vScale.m128_f32[2] = SafeScaleToOne(vScale.m128_f32[2]);


                                    _matrix matRotation = XMMatrixRotationQuaternion(vRotationOut);

                                    _matrix matScale = XMMatrixScaling(XMVectorGetX(vScale),
                                        XMVectorGetY(vScale),
                                        XMVectorGetZ(vScale));

                                    _matrix matPosition = XMMatrixTranslation(XMVectorGetX(vLocation), XMVectorGetY(vLocation), XMVectorGetZ(vLocation));

                                    matWorld = matScale * matRotation * matPosition;
                                }
                            }



                        }
                        // 위치

                        _vector vLocalPosition = {};
                        if (props.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
                            auto loc = props["RelativeLocation"];
                            _float fPositionX = loc["X"] * 0.01f;
                            _float fPositionY = loc["Y"] * -0.01f;
                            _float fPositionZ = loc["Z"] * 0.01f;

                            vLocalPosition = XMVectorSet(fPositionX, fPositionZ, fPositionY, 0.f);
                        }

                        // 회전 (절대!!! 건들지 말것)
                        _vector vLocalRotation = {};
                        if (props.contains("RelativeRotation")) {
                            auto rot = props["RelativeRotation"];
                            _float rotPitch = rot["Pitch"];
                            _float rotYaw = rot["Yaw"];
                            _float rotRoll = rot["Roll"];

                            _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

                            vRotation.m128_f32[2] *= -1.f;

                            XMVECTOR vAdditionalRotation = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(180.0f));


                            XMVECTOR vNewRotation = XMQuaternionMultiply(vRotation, vAdditionalRotation);

                            _float fX = XMVectorGetX(vNewRotation); // 원래 Z축으로 90도 회전을 해야하는데 XMQuaternionMultiply로 Z축 90도 회전하면 값이 이상해짐..
                            _float fZ = XMVectorGetZ(vNewRotation); // X와 Z값 서로 바꿔주기

                            vNewRotation.m128_f32[0] = fZ;
                            vNewRotation.m128_f32[2] = fX;

                            vLocalRotation = vNewRotation;

                        }

                        // 스케일
                        _vector vLocalScale = {};
                        if (props.contains("RelativeScale3D")) {
                            auto scale = props["RelativeScale3D"];
                            _float fScaleX = scale.contains("X") ? SafeScale(scale["X"].get<float>()) : 1.0f;
                            _float fScaleY = scale.contains("Y") ? SafeScale(scale["Y"].get<float>()) : 1.0f;
                            _float fScaleZ = scale.contains("Z") ? SafeScale(scale["Z"].get<float>()) : 1.0f;

                            fScaleX *= 0.01f;
                            fScaleY *= 0.01f;
                            fScaleZ *= 0.01f;

                            fScaleX = SafeScale(fScaleX);
                            fScaleY = SafeScale(fScaleY);
                            fScaleZ = SafeScale(fScaleZ);


                            vLocalScale = XMVectorSet(fScaleX, fScaleZ, fScaleY, 0.f); // Z-Up기준 sclae값 Y-Up으로 변경
                        }

                        vLocalScale.m128_f32[0] = SafeScale(vLocalScale.m128_f32[0]); // 현재 스케일 값이 1일경우 Json에 값을 저장하지 않으므로 scale이 0일 경우 0.01(m -> cm)로 바꿔줘야함
                        vLocalScale.m128_f32[1] = SafeScale(vLocalScale.m128_f32[1]);
                        vLocalScale.m128_f32[2] = SafeScale(vLocalScale.m128_f32[2]);

                        _matrix matLocalRotation = XMMatrixRotationQuaternion(vLocalRotation);

                        _matrix matLocalScale = XMMatrixScaling(XMVectorGetX(vLocalScale),
                            XMVectorGetY(vLocalScale),
                            XMVectorGetZ(vLocalScale));

                        _matrix matLocalPosition = XMMatrixTranslation(XMVectorGetX(vLocalPosition), XMVectorGetY(vLocalPosition), XMVectorGetZ(vLocalPosition));

                        _matrix matLocal = matLocalScale * matLocalRotation * matLocalPosition;

                        matWorld = matLocal * matWorld; // 월드 위치와 로컬 위치 곱 (월드는 기본적으로 identity라 부모가 없을경우 그대로 출력

                        _vector vOutPos, vOutRot, vOutScale;

                        XMMatrixDecompose(&vOutScale, &vOutRot, &vOutPos, matWorld); // 로컬 * 월드한 transform

                        XMStoreFloat3(&info.fPosition, vOutPos);
                        XMStoreFloat3(&info.fScale, vOutScale);
                        XMStoreFloat4(&info.fRotation, vOutRot);

                        results.push_back(info);

                    }


                    if (ChildProp.contains("BlueprintCreatedComponents")) // 자식이 StaticMesh 인지 InstanceMesh인지 splineMesh인지 등등.. 확인하기
                    { 
                        const auto BPComponents = ChildProp["BlueprintCreatedComponents"];

                        _vector vParentScale = XMVectorSet(1.f, 1.f, 1.f, 0.f);
                        _vector vParentRotation = XMVectorSet(0.f, 0.f, 0.f, 1.f);

                        _bool bParentSclaeExist = false;
                        _bool bParentRotationExist = false;

                        if (iter.contains("Properties"))
                        {
                            const auto& ChildPropProp = iter["Properties"];
                           
                            if (ChildPropProp.contains("ScaleBase"))
                            {
                                const auto& ChildPropScale = ChildPropProp["ScaleBase"];

                                vParentScale.m128_f32[0] = SafeScaleToOne(ChildPropScale["X"]);
                                vParentScale.m128_f32[1] = SafeScaleToOne(ChildPropScale["Z"]);
                                vParentScale.m128_f32[2] = SafeScaleToOne(ChildPropScale["Y"]);

                                bParentSclaeExist = true;
                            }

                            if (ChildPropProp.contains("RotationBase"))
                            {
                                const auto& ChildPropRotation = ChildPropProp["RotationBase"];

                                _float fPitch = ChildPropRotation["Pitch"];
                                _float fYaw = ChildPropRotation["Yaw"];
                                _float fRoll= ChildPropRotation["Roll"];

                                vParentRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(fPitch), XMConvertToRadians(fYaw), XMConvertToRadians(fRoll));
                                bParentRotationExist = true;
                            }
                            //if(ChildPropProp.contains("")
                        }


                        for (const auto& BPComponent : BPComponents)
                        {

                            if (!BPComponent.contains("ObjectPath")) continue;

                            int Childindex = ExtractIndexFast(BPComponent["ObjectPath"]);

                            const auto& ChildChildProp = mapIndex[Childindex];

                            regex indexRegex(R"(\.([0-9]+)$)"); // 인덱스 찾기용 변수들
                            smatch match2;

                            const auto& props = ChildChildProp["Properties"];
                        
                            if(ChildChildProp["Type"] != "SceneComponent" && ChildChildProp["Type"] != "BoxComponent")
                            {
                          /*  if (ChildChildProp.contains("Type") && ChildChildProp["Type"] == "HierarchicalInstancedStaticMeshComponent")
                            {*/
                                vector <string> parentObjectNames;
                                _matrix matWorld = XMMatrixIdentity();

                                vector <_matrix> parentBluePrintedMatrix;

                                if (ChildChildProp.contains("Properties"))
                                {
                                    const auto& ParentProp = ChildChildProp["Properties"];

                                    if (ParentProp.contains("AttachParent")) // 부모 transform 가져오기
                                    {
                                        string parentName = ExtractSimpleName(ChildChildProp["Properties"]["AttachParent"]["ObjectPath"]);

                                        smatch match2;
                                        if (std::regex_search(parentName, match2, indexRegex)) {
                                            int index = std::stoi(match2[1].str());

                                            const auto& iter3 = mapIndex[index];

                                            if (iter3.contains("Properties"))
                                            {
                                                const auto& parentObject = iter3["Properties"];

                                                _vector vLocation = {};
                                                if (parentObject.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
                                                    auto loc = parentObject["RelativeLocation"];
                                                    vLocation.m128_f32[0] = loc["X"] * 0.01f;
                                                    vLocation.m128_f32[1] = loc["Z"] * 0.01f;
                                                    vLocation.m128_f32[2] = loc["Y"] * -0.01f;
                                                    vLocation.m128_f32[3] = 1.f;
                                                }

                                                _vector vRotationOut = {};
                                                if (parentObject.contains("RelativeRotation")) {
                                                    auto rot = parentObject["RelativeRotation"];
                                                    _float rotPitch = rot["Pitch"];
                                                    _float rotYaw = rot["Yaw"];
                                                    _float rotRoll = rot["Roll"];

                                                    _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

                                                    vRotation.m128_f32[2] *= -1.f;

                                                    _float fX = XMVectorGetX(vRotation);
                                                    _float fZ = XMVectorGetZ(vRotation);

                                                    vRotation.m128_f32[0] = fZ;
                                                    vRotation.m128_f32[2] = fX;

                                                    vRotationOut = vRotation;
                                                }

                                                _vector vScale = {};
                                                // 스케일
                                                if (parentObject.contains("RelativeScale3D")) {
                                                    auto scale = parentObject["RelativeScale3D"];
                                                    _float fScaleX = scale.contains("X");
                                                    _float fScaleY = scale.contains("Y");
                                                    _float fScaleZ = scale.contains("Z");

                                                    fScaleX = SafeScaleToOne(fScaleX);
                                                    fScaleY = SafeScaleToOne(fScaleY);
                                                    fScaleZ = SafeScaleToOne(fScaleZ);

                                                    vScale = XMVectorSet(fScaleX, fScaleY, fScaleZ, 0.f);
                                                }
                                                vScale.m128_f32[0] = SafeScaleToOne(vScale.m128_f32[0]); // 현재 스케일 값이 1일경우 Json에 값을 저장하지 않으므로 scale이 0일 경우 0.01(m -> cm)로 바꿔줘야함
                                                vScale.m128_f32[1] = SafeScaleToOne(vScale.m128_f32[1]);
                                                vScale.m128_f32[2] = SafeScaleToOne(vScale.m128_f32[2]);


                                                _matrix matRotation = XMMatrixRotationQuaternion(vRotationOut);

                                                _matrix matScale = XMMatrixScaling(XMVectorGetX(vScale),
                                                    XMVectorGetY(vScale),
                                                    XMVectorGetZ(vScale));

                                                _matrix matPosition = XMMatrixTranslation(XMVectorGetX(vLocation), XMVectorGetY(vLocation), XMVectorGetZ(vLocation));

                                                matWorld = matScale * matRotation * matPosition;
                                            }

                                            //메쉬 이름 구해오기(보통 Blueprint나 부모에 저장되어 있음)

                                            if (iter.contains("Properties"))
                                            {
                                                const auto& ChildName = iter["Properties"];

                                                if (ParentProp.contains("StaticMesh"))
                                                {
                                                    string strMeshName = GetFileNameFromPath(ParentProp["StaticMesh"]["ObjectPath"]);
                                                    string FinalName = StripNumberSuffix(strMeshName);
                                                    parentObjectNames.push_back(FinalName);
                                                }
                                                if (iter.contains("Template"))
                                                {
                                                    string ParentName = GetFileNameFromPath(iter["Template"]["ObjectPath"]);
                                                    string BPName = StripNumberSuffix(ParentName);

                                                    string strVariableName = ExtractVariableName(ChildChildProp["Template"]["ObjectName"]);

                                                    string strParentFinalPath = "../Bin/DataFiles/Jsons/" + BPName + ".json";

                                                    std::ifstream parentfile(strParentFinalPath);
                                                    if (!parentfile.is_open()) {
                                                        std::cerr << BPName.c_str() << " is Undetectable! \n";
                                                        continue;
                                                    }
                                                    else
                                                    {
                                                        std::cerr << BPName.c_str() << " is Loaded \n";
                                                    }

                                                    json Parentj;

                                                    parentfile >> Parentj;

                                                    for (const auto& parentobj : Parentj)
                                                    {
                                                        if (parentobj.contains("Type") && parentobj["Type"] == "HierarchicalInstancedStaticMeshComponent") {

                                                            if (parentobj["Name"] != strVariableName)   continue;

                                                            const auto& parentprops = parentobj["Properties"];

                                                            if (parentprops.contains("StaticMesh"))
                                                            {
                                                                const auto& NameProp = parentprops["StaticMesh"];

                                                                if (NameProp.contains("ObjectName"))
                                                                    parentObjectNames.push_back(ExtractQuotedText(parentprops["StaticMesh"]["ObjectName"]));
                                                            }
                                                        }

                                                        if (parentobj.contains("Type") && parentobj["Type"] == "StaticMeshComponent") {

                                                            if (parentobj["Name"] != strVariableName)   continue;

                                                            const auto& parentprops = parentobj["Properties"];

                                                            if (parentprops.contains("StaticMesh"))
                                                            {
                                                                const auto& NameProp = parentprops["StaticMesh"];

                                                                if (NameProp.contains("ObjectName"))
                                                                    parentObjectNames.push_back(ExtractQuotedText(parentprops["StaticMesh"]["ObjectName"]));
                                                            }
                                                        }

                                                        if (parentobj.contains("Type") && parentobj["Type"] == BPName + "_C")
                                                        {
                                                            if (parentobj.contains("Properties"))
                                                            {
                                                                const auto& BPProps = parentobj["Properties"];

                                                                _int iRow = { 0 };
                                                                _int iColumn = { 0 };

                                                                if (BPProps.contains("Rows"))
                                                                    iRow = BPProps["Rows"];

                                                                if (BPProps.contains("Columns"))
                                                                    iColumn = BPProps["Columns"];

                                                                _float3 LocationOffset = {};

                                                                if (BPProps.contains("LocationOffset"))
                                                                {
                                                                    auto LOffset = BPProps["LocationOffset"];

                                                                    LocationOffset = _float3(LOffset["X"] * 0.01f, LOffset["Y"] * -0.01f, LOffset["Z"] * 0.01f);
                                                                }

                                                                _float3 ScaleBase = { 1.f, 1.f, 1.f };
                                                                if (BPProps.contains("ScaleBase"))
                                                                {
                                                                    auto LScaleBase = BPProps["ScaleBase"];

                                                                    _float fXScale = LScaleBase["X"];
                                                                    _float fYScale = LScaleBase["Y"];
                                                                    _float fZScale = LScaleBase["Z"];

                                                                    fXScale = SafeScaleToOne(fXScale);
                                                                    fYScale = SafeScaleToOne(fYScale);
                                                                    fZScale = SafeScaleToOne(fZScale);

                                                                    ScaleBase = _float3(fXScale, fYScale, fZScale);
                                                                }

                                                                _vector RotationBase = {};
                                                                if (BPProps.contains("RotationBase"))
                                                                {
                                                                    auto LRotationBase = BPProps["RotationBase"];

                                                                    _float rotPitch = LRotationBase["Pitch"];
                                                                    _float rotYaw = LRotationBase["Yaw"];
                                                                    _float rotRoll = LRotationBase["Roll"];

                                                                    if (rotPitch <= 0.0001f && rotYaw <= 0.0001f && rotRoll <= 0.0001f)
                                                                    {
                                                                        RotationBase = XMVectorSet(0.f, 0.f, 0.f, 1.f);
                                                                    }
                                                                    else
                                                                    {
                                                                        _vector vRot = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

                                                                        /* _vector vRotation = XMQuaternionRotationAxis(XMVectorSet(0.f, 0.f, 1.f, 0.f), XMConvertToRadians(90.f));

                                                                         vRot = XMQuaternionMultiply(vRotation, vRot);*/

                                                                        RotationBase = vRot;
                                                                    }

                                                                }

                                                                _int centerRow = iRow / 2;
                                                                _int centerColumn = iColumn / 2;


                                                                _matrix matTranslation = XMMatrixIdentity();

                                                                _matrix matRotation = {};
                                                                if (!bParentRotationExist)
                                                                {
                                                                    matRotation = XMMatrixRotationQuaternion(RotationBase);
                                                                }
                                                                else
                                                                    matRotation = XMMatrixRotationQuaternion(vParentRotation);

                                                                _matrix matScale = {};
                                                                if (!bParentSclaeExist)
                                                                    matScale = XMMatrixScaling(ScaleBase.x, ScaleBase.y, ScaleBase.z);
                                                                else
                                                                    matScale = XMMatrixScaling(XMVectorGetX(vParentScale), XMVectorGetY(vParentScale), XMVectorGetZ(vParentScale));

                                                                _matrix matWorld = matScale * XMMatrixIdentity() * matTranslation;

                                                                matWorld = matWorld * matRotation;

                                                                parentBluePrintedMatrix.push_back(matWorld);
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                        }
                                    }


                                }

                                if (ChildChildProp.contains("PerInstanceSMData"))
                                {

                                    for (const auto& instance : ChildChildProp["PerInstanceSMData"]) {
                                        if (instance.contains("TransformData")) {
                                            const auto& transform = instance["TransformData"];

                                            // 접근 예시
                                            MESHINFO_DESC info;

                                            _matrix matLocal = {};

                                            if (parentObjectNames.size() > 0)
                                            {
                                                info.ObjectName = parentObjectNames[0];

                                                _vector vLocalTranslation = {};
                                                if (transform.contains("Translation"))
                                                {
                                                    auto translation = transform["Translation"];

                                                    _float fPositionX = translation["X"] * 0.01f;
                                                    _float fPositionY = translation["Y"] * -0.01f;
                                                    _float fPositionZ = translation["Z"] * 0.01f;

                                                    vLocalTranslation = XMVectorSet(fPositionX, fPositionZ, fPositionY, 0.f);
                                                }

                                                _vector vLocalRotation = {};
                                                if (transform.contains("Rotation")) {
                                                    auto rotation = transform["Rotation"];
                                                    _float rotX = rotation["X"];
                                                    _float rotY = rotation["Y"];
                                                    _float rotZ = rotation["Z"];
                                                    _float rotW = rotation["W"];

                                                    _vector vRotation = XMVectorSet(rotX, rotY, rotZ, rotW);

                                                    vLocalRotation = vRotation;
                                                }
                                                _vector vLocalScale = {};
                                                if (transform.contains("Scale3D")) {
                                                    auto scale = transform["Scale3D"];
                                                    _float fScaleX = scale.contains("X") ? SafeScale(scale["X"].get<float>()) : 1.0f;
                                                    _float fScaleY = scale.contains("Y") ? SafeScale(scale["Y"].get<float>()) : 1.0f;
                                                    _float fScaleZ = scale.contains("Z") ? SafeScale(scale["Z"].get<float>()) : 1.0f;

                                                    fScaleX *= 0.01f;
                                                    fScaleY *= 0.01f;
                                                    fScaleZ *= 0.01f;

                                                    fScaleX = SafeScale(fScaleX);
                                                    fScaleY = SafeScale(fScaleY);
                                                    fScaleZ = SafeScale(fScaleZ);


                                                    vLocalScale = XMVectorSet(fScaleX, fScaleZ, fScaleY, 0.f);
                                                }

                                                vLocalScale.m128_f32[0] = SafeScale(vLocalScale.m128_f32[0]);
                                                vLocalScale.m128_f32[1] = SafeScale(vLocalScale.m128_f32[1]);
                                                vLocalScale.m128_f32[2] = SafeScale(vLocalScale.m128_f32[2]);

                                                _matrix matLocalRotation = XMMatrixRotationQuaternion(vLocalRotation);

                                                _matrix matLocalScale = XMMatrixScaling(XMVectorGetX(vLocalScale),
                                                    XMVectorGetY(vLocalScale),
                                                    XMVectorGetZ(vLocalScale));

                                                _matrix matLocalPosition = XMMatrixTranslation(XMVectorGetX(vLocalTranslation), XMVectorGetY(vLocalTranslation), XMVectorGetZ(vLocalTranslation));

                                                matLocal = matLocalScale * matLocalRotation * matLocalPosition;

                                                _matrix matOut = matWorld; // matWorld바뀌지 않도록하기

                                                if (parentBluePrintedMatrix.size() > 0)
                                                {
                                                    _matrix matDesc = parentBluePrintedMatrix.front() * matLocal * matOut;

                                                    _vector vOutPos, vOutRot, vOutScale;

                                                    XMMatrixDecompose(&vOutScale, &vOutRot, &vOutPos, matDesc);

                                                    /*       _float fY = vOutScale.m128_f32[1];
                                                           _float fZ = vOutScale.m128_f32[2];

                                                           vOutScale.m128_f32[2] = fZ;
                                                           vOutScale.m128_f32[1] = fY;*/

                                                    XMStoreFloat3(&info.fPosition, vOutPos);
                                                    XMStoreFloat3(&info.fScale, vOutScale);
                                                    XMStoreFloat4(&info.fRotation, vOutRot);

                                                    results.push_back(info);
                                                }
                                                else
                                                {
                                                    _matrix matDesc = matLocal * matOut;

                                                    _vector vOutPos, vOutRot, vOutScale;

                                                    XMMatrixDecompose(&vOutScale, &vOutRot, &vOutPos, matDesc);

                                                    XMStoreFloat3(&info.fPosition, vOutPos);
                                                    XMStoreFloat3(&info.fScale, vOutScale);
                                                    XMStoreFloat4(&info.fRotation, vOutRot);

                                                    results.push_back(info);
                                                }
                                            }
                                        }
                                    }
                                }
                                if ((ChildChildProp.contains("Name") && ChildChildProp["Name"] == "StaticMesh") || (ChildChildProp.contains("Type") && ChildChildProp["Type"] == "StaticMeshComponent"))
                                {
                                    if (!ChildChildProp.contains("Properties")) continue;

                                    if (parentObjectNames.size() != 0)
                                    {
                                        MESHINFO_DESC info;

                                        info.ObjectName = parentObjectNames.front();

                                        _matrix matStaticWorld = XMMatrixIdentity();
                                        _matrix matParentWorld = XMMatrixIdentity();
                                        _matrix matParentParentWorld = XMMatrixIdentity();
                                        _matrix matParentparentparentWorld = XMMatrixIdentity();

                                        if (ChildChildProp["Properties"].contains("AttachParent"))
                                        {
                                            string parentName = ExtractSimpleName(ChildChildProp["Properties"]["AttachParent"]["ObjectPath"]);

                                            if (std::regex_search(parentName, match2, indexRegex)) { // 부모 인덱스로 찾아오기
                                                int index = std::stoi(match2[1].str());

                                                const auto& staticMesh = mapIndex[index];

                                                if (staticMesh.contains("Properties") && staticMesh["Properties"].contains("AttachParent")) // 부모 찾기 현재 메쉬에 부모 블루프린트 Transform에 곱하는 형식이 아니면 쓸모 X
                                                {
                                                    string StaticparentName = ExtractSimpleName(staticMesh["Properties"]["AttachParent"]["ObjectPath"]);

                                                    if (std::regex_search(StaticparentName, match2, indexRegex)) { // 부모 인덱스로 찾아오기
                                                        int index = std::stoi(match2[1].str());

                                                        const auto& staticparent = mapIndex[index];

                                                        if (staticparent.contains("Properties"))
                                                        {
                                                            if (staticparent["Properties"].contains("AttachParent"))
                                                            {

                                                                string parentparentName = ExtractSimpleName(staticparent["Properties"]["AttachParent"]["ObjectPath"]);

                                                                if (std::regex_search(parentparentName, match2, indexRegex)) { // 부모 인덱스로 찾아오기
                                                                    int parentindex = std::stoi(match2[1].str());

                                                                    const auto& staticparentparentMesh = mapIndex[parentindex];

                                                                    if (staticparentparentMesh.contains("Properties"))
                                                                    {
                                                                        /* if (staticparentparentMesh["Properties"].contains("AttachParent"))
                                                                        {

                                                                        }*/

                                                                        const auto& parentparentMeshProp = staticparentparentMesh["Properties"];

                                                                        _vector vParentLocation = {};
                                                                        if (parentparentMeshProp.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
                                                                            auto loc = parentparentMeshProp["RelativeLocation"];
                                                                            vParentLocation.m128_f32[0] = loc["X"] * 0.01f;
                                                                            vParentLocation.m128_f32[1] = loc["Z"] * 0.01f;
                                                                            vParentLocation.m128_f32[2] = loc["Y"] * -0.01f;
                                                                            vParentLocation.m128_f32[3] = 1.f;
                                                                        }

                                                                        _vector vParentRotationOut = {};
                                                                        if (parentparentMeshProp.contains("RelativeRotation")) { // 회전 따로 Z-UP to Y-UP 적용하지 않음
                                                                            auto rot = parentparentMeshProp["RelativeRotation"];
                                                                            _float rotPitch = rot["Pitch"];
                                                                            _float rotYaw = rot["Yaw"];
                                                                            _float rotRoll = rot["Roll"];

                                                                            _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

                                                                            vParentRotationOut = vRotation;
                                                                        }

                                                                        _vector vParentScale = {};
                                                                        // 스케일
                                                                        if (parentparentMeshProp.contains("RelativeScale3D")) { // 크기 보통 0으로 되있으므로 1로 설정
                                                                            auto scale = parentparentMeshProp["RelativeScale3D"];
                                                                            _float fScaleX = scale.contains("X");
                                                                            _float fScaleY = scale.contains("Y");
                                                                            _float fScaleZ = scale.contains("Z");

                                                                            fScaleX = SafeScaleToOne(fScaleX);
                                                                            fScaleY = SafeScaleToOne(fScaleY);
                                                                            fScaleZ = SafeScaleToOne(fScaleZ);

                                                                            vParentScale = XMVectorSet(fScaleX, fScaleY, fScaleZ, 0.f);
                                                                        }
                                                                        vParentScale.m128_f32[0] = SafeScaleToOne(vParentScale.m128_f32[0]);
                                                                        vParentScale.m128_f32[1] = SafeScaleToOne(vParentScale.m128_f32[1]);
                                                                        vParentScale.m128_f32[2] = SafeScaleToOne(vParentScale.m128_f32[2]);


                                                                        _matrix matParentRotation = XMMatrixRotationQuaternion(vParentRotationOut);

                                                                        _matrix matParentScale = XMMatrixScaling(XMVectorGetX(vParentScale),
                                                                            XMVectorGetY(vParentScale),
                                                                            XMVectorGetZ(vParentScale));

                                                                        _matrix matParentPosition = XMMatrixTranslation(XMVectorGetX(vParentLocation), XMVectorGetY(vParentLocation), XMVectorGetZ(vParentLocation));

                                                                        matParentParentWorld = matParentScale * matParentRotation * matParentPosition;
                                                                    }

                                                                }
                                                            }
                                                            const auto& parentObject = staticparent["Properties"];

                                                            _vector vLocation = {};
                                                            if (parentObject.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
                                                                auto loc = parentObject["RelativeLocation"];
                                                                vLocation.m128_f32[0] = loc["X"] * 0.01f;
                                                                vLocation.m128_f32[1] = loc["Z"] * 0.01f;
                                                                vLocation.m128_f32[2] = loc["Y"] * -0.01f;
                                                                vLocation.m128_f32[3] = 1.f;
                                                            }

                                                            _vector vRotationOut = {};
                                                            if (parentObject.contains("RelativeRotation")) { // 회전 따로 Z-UP to Y-UP 적용하지 않음
                                                                auto rot = parentObject["RelativeRotation"];
                                                                _float rotPitch = rot["Pitch"];
                                                                _float rotYaw = rot["Yaw"];
                                                                _float rotRoll = rot["Roll"];

                                                                _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

                                                                vRotationOut = vRotation;
                                                            }

                                                            _vector vScale = {};
                                                            // 스케일
                                                            if (parentObject.contains("RelativeScale3D")) { // 크기 보통 0으로 되있으므로 1로 설정
                                                                auto scale = parentObject["RelativeScale3D"];
                                                                _float fScaleX = scale.contains("X");
                                                                _float fScaleY = scale.contains("Y");
                                                                _float fScaleZ = scale.contains("Z");

                                                                fScaleX = SafeScaleToOne(fScaleX);
                                                                fScaleY = SafeScaleToOne(fScaleY);
                                                                fScaleZ = SafeScaleToOne(fScaleZ);

                                                                vScale = XMVectorSet(fScaleX, fScaleY, fScaleZ, 0.f);
                                                            }
                                                            vScale.m128_f32[0] = SafeScaleToOne(vScale.m128_f32[0]);
                                                            vScale.m128_f32[1] = SafeScaleToOne(vScale.m128_f32[1]);
                                                            vScale.m128_f32[2] = SafeScaleToOne(vScale.m128_f32[2]);


                                                            _matrix matRotation = XMMatrixRotationQuaternion(vRotationOut);

                                                            _matrix matScale = XMMatrixScaling(XMVectorGetX(vScale),
                                                                XMVectorGetY(vScale),
                                                                XMVectorGetZ(vScale));

                                                            _matrix matPosition = XMMatrixTranslation(XMVectorGetX(vLocation), XMVectorGetY(vLocation), XMVectorGetZ(vLocation));

                                                            matParentWorld = matScale * matRotation * matPosition;
                                                        }
                                                    }



                                                }
                                                // 위치

                                                const auto& staticMeshProp = staticMesh["Properties"];

                                                _vector vStaticPosition = {};
                                                if (staticMeshProp.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
                                                    auto loc = staticMeshProp["RelativeLocation"];
                                                    _float fPositionX = loc["X"] * 0.01f;
                                                    _float fPositionY = loc["Y"] * -0.01f;
                                                    _float fPositionZ = loc["Z"] * 0.01f;

                                                    vStaticPosition = XMVectorSet(fPositionX, fPositionZ, fPositionY, 0.f);
                                                }

                                                // 회전 (절대!!! 건들지 말것)
                                                _vector vStaticRotation = {};
                                                if (staticMeshProp.contains("RelativeRotation")) {
                                                    auto rot = staticMeshProp["RelativeRotation"];
                                                    _float rotPitch = rot["Pitch"];
                                                    _float rotYaw = rot["Yaw"];
                                                    _float rotRoll = rot["Roll"];

                                                    _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

                                                    //vRotation.m128_f32[2] *= -1.f;

                                                    //XMVECTOR vAdditionalRotation = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(180.0f));


                                                    //XMVECTOR vNewRotation = XMQuaternionMultiply(vRotation, vAdditionalRotation);

                                                    //_float fX = XMVectorGetX(vNewRotation); // 원래 Z축으로 90도 회전을 해야하는데 XMQuaternionMultiply로 Z축 90도 회전하면 값이 이상해짐..
                                                    //_float fZ = XMVectorGetZ(vNewRotation); // X와 Z값 서로 바꿔주기

                                                    //vNewRotation.m128_f32[0] = fZ;
                                                    //vNewRotation.m128_f32[2] = fX;

                                                    vStaticRotation = vRotation;

                                                }

                                                // 스케일
                                                _vector vStaticScale = {};
                                                if (staticMeshProp.contains("RelativeScale3D")) {
                                                    auto scale = staticMeshProp["RelativeScale3D"];
                                                    _float fScaleX = scale.contains("X") ? SafeScaleToOne(scale["X"].get<float>()) : 1.0f;
                                                    _float fScaleY = scale.contains("Y") ? SafeScaleToOne(scale["Y"].get<float>()) : 1.0f;
                                                    _float fScaleZ = scale.contains("Z") ? SafeScaleToOne(scale["Z"].get<float>()) : 1.0f;

                                                    //fScaleX *= 0.01f;
                                                    //fScaleY *= 0.01f;
                                                    //fScaleZ *= 0.01f;

                                                    fScaleX = SafeScaleToOne(fScaleX);
                                                    fScaleY = SafeScaleToOne(fScaleY);
                                                    fScaleZ = SafeScaleToOne(fScaleZ);


                                                    vStaticScale = XMVectorSet(fScaleX, fScaleZ, fScaleY, 0.f); // Z-Up기준 sclae값 Y-Up으로 변경
                                                }

                                                vStaticScale.m128_f32[0] = SafeScaleToOne(vStaticScale.m128_f32[0]); // 현재 스케일 값이 1일경우 Json에 값을 저장하지 않으므로 scale이 0일 경우 0.01(m -> cm)로 바꿔줘야함
                                                vStaticScale.m128_f32[1] = SafeScaleToOne(vStaticScale.m128_f32[1]);
                                                vStaticScale.m128_f32[2] = SafeScaleToOne(vStaticScale.m128_f32[2]);

                                                _matrix matStaticRotation = XMMatrixRotationQuaternion(vStaticRotation);

                                                _matrix matStaticScale = XMMatrixScaling(XMVectorGetX(vStaticScale),
                                                    XMVectorGetY(vStaticScale),
                                                    XMVectorGetZ(vStaticScale));

                                                _matrix matStaticPosition = XMMatrixTranslation(XMVectorGetX(vStaticPosition), XMVectorGetY(vStaticPosition), XMVectorGetZ(vStaticPosition));

                                                matStaticWorld = matStaticScale * matStaticRotation * matStaticPosition;
                                            }
                                        }

                                        const auto& FinalProp = ChildChildProp["Properties"];

                                        _vector vLocalPosition = {};
                                        if (FinalProp.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
                                            auto loc = FinalProp["RelativeLocation"];
                                            _float fPositionX = loc["X"] * 0.01f;
                                            _float fPositionY = loc["Y"] * -0.01f;
                                            _float fPositionZ = loc["Z"] * 0.01f;

                                            vLocalPosition = XMVectorSet(fPositionX, fPositionZ, fPositionY, 0.f);
                                        }

                                        // 회전 (절대!!! 건들지 말것)
                                        _vector vLocalRotation = {};
                                        if (FinalProp.contains("RelativeRotation")) {
                                            auto rot = FinalProp["RelativeRotation"];
                                            _float rotPitch = rot["Pitch"];
                                            _float rotYaw = rot["Yaw"];
                                            _float rotRoll = rot["Roll"];

                                            _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

                                            /*vRotation.m128_f32[2] *= -1.f;

                                            XMVECTOR vAdditionalRotation = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(180.0f));


                                            XMVECTOR vNewRotation = XMQuaternionMultiply(vRotation, vAdditionalRotation);*/

                                            //_float fX = XMVectorGetX(vNewRotation); // 원래 Z축으로 90도 회전을 해야하는데 XMQuaternionMultiply로 Z축 90도 회전하면 값이 이상해짐..
                                            //_float fZ = XMVectorGetZ(vNewRotation); // X와 Z값 서로 바꿔주기

                                            //vNewRotation.m128_f32[0] = fZ;
                                            //vNewRotation.m128_f32[2] = fX;

                                            vLocalRotation = vRotation;

                                        }

                                        // 스케일
                                        _vector vLocalScale = {};
                                        if (FinalProp.contains("RelativeScale3D")) {
                                            auto scale = FinalProp["RelativeScale3D"];
                                            _float fScaleX = scale.contains("X") ? SafeScaleToOne(scale["X"].get<float>()) : 1.0f;
                                            _float fScaleY = scale.contains("Y") ? SafeScaleToOne(scale["Y"].get<float>()) : 1.0f;
                                            _float fScaleZ = scale.contains("Z") ? SafeScaleToOne(scale["Z"].get<float>()) : 1.0f;

                                           /* fScaleX *= 0.01f;
                                            fScaleY *= 0.01f;
                                            fScaleZ *= 0.01f;*/

                                            fScaleX = SafeScaleToOne(fScaleX);
                                            fScaleY = SafeScaleToOne(fScaleY);
                                            fScaleZ = SafeScaleToOne(fScaleZ);


                                            vLocalScale = XMVectorSet(fScaleX, fScaleZ, fScaleY, 0.f); // Z-Up기준 sclae값 Y-Up으로 변경
                                        }

                                        vLocalScale.m128_f32[0] = SafeScaleToOne(vLocalScale.m128_f32[0]); // 현재 스케일 값이 1일경우 Json에 값을 저장하지 않으므로 scale이 0일 경우 0.01(m -> cm)로 바꿔줘야함
                                        vLocalScale.m128_f32[1] = SafeScaleToOne(vLocalScale.m128_f32[1]);
                                        vLocalScale.m128_f32[2] = SafeScaleToOne(vLocalScale.m128_f32[2]);

                                        _matrix matLocalRotation = XMMatrixRotationQuaternion(vLocalRotation);

                                        _matrix matLocalScale = XMMatrixScaling(XMVectorGetX(vLocalScale),
                                            XMVectorGetY(vLocalScale),
                                            XMVectorGetZ(vLocalScale));

                                        _matrix matLocalPosition = XMMatrixTranslation(XMVectorGetX(vLocalPosition), XMVectorGetY(vLocalPosition), XMVectorGetZ(vLocalPosition));

                                        _matrix matLocal = matLocalScale * matLocalRotation * matLocalPosition;

                                        _matrix matFinal = matLocal * matStaticWorld * matParentWorld * matParentParentWorld; // 월드 위치와 로컬 위치 곱 (월드는 기본적으로 identity라 부모가 없을경우 그대로 출력

                                        _vector vOutPos, vOutRot, vOutScale;

                                        XMMatrixDecompose(&vOutScale, &vOutRot, &vOutPos, matFinal); // 로컬 * 월드한 transform

                                        for (size_t i = 0; i < 3; ++i)
                                            vOutScale.m128_f32[i] *= 0.01f;

                                        vOutRot.m128_f32[2] *= -1.f;

                                        _vector vOutAdditionalRotation = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(180.0f));

                                        _vector vOutNewRotation = XMQuaternionMultiply(vOutRot, vOutAdditionalRotation);

                                        _float fX = XMVectorGetX(vOutNewRotation); // 원래 Z축으로 90도 회전을 해야하는데 XMQuaternionMultiply로 Z축 90도 회전하면 값이 이상해짐..
                                        _float fZ = XMVectorGetZ(vOutNewRotation); // X와 Z값 서로 바꿔주기

                                        vOutNewRotation.m128_f32[0] = fZ;
                                        vOutNewRotation.m128_f32[2] = fX;

                                        XMStoreFloat3(&info.fPosition, vOutPos);
                                        XMStoreFloat3(&info.fScale, vOutScale);
                                        XMStoreFloat4(&info.fRotation, vOutNewRotation);

                                        results.push_back(info);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }


    }
    
   // for (const auto& obj : j)
   // {
       
        //if (obj.contains("Type") && obj["Type"] == "HierarchicalInstancedStaticMeshComponent")
        //{
        //    vector <string> parentObjectNames;
        //    _matrix matWorld = XMMatrixIdentity();

        //    vector <_matrix> parentBluePrintedMatrix;

        //    regex indexRegex(R"(\.([0-9]+)$)");

        //    if (obj.contains("Properties"))
        //    {
        //        const auto& ParentProp = obj["Properties"];

        //        if (ParentProp.contains("AttachParent")) // 부모 transform 가져오기
        //        {
        //            string parentName = ExtractSimpleName(obj["Properties"]["AttachParent"]["ObjectPath"]);

        //            smatch match2;
        //            if (std::regex_search(parentName, match2, indexRegex)) {
        //                int index = std::stoi(match2[1].str());

        //                const auto& iter = mapIndex[index];

        //                if (iter.contains("Properties"))
        //                {
        //                    const auto& parentObject = iter["Properties"];

        //                    _vector vLocation = {};
        //                    if (parentObject.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
        //                        auto loc = parentObject["RelativeLocation"];
        //                        vLocation.m128_f32[0] = loc["X"] * 0.01f;
        //                        vLocation.m128_f32[1] = loc["Z"] * 0.01f;
        //                        vLocation.m128_f32[2] = loc["Y"] * -0.01f;
        //                        vLocation.m128_f32[3] = 1.f;
        //                    }

        //                    _vector vRotationOut = {};
        //                    if (parentObject.contains("RelativeRotation")) {
        //                        auto rot = parentObject["RelativeRotation"];
        //                        _float rotPitch = rot["Pitch"];
        //                        _float rotYaw = rot["Yaw"];
        //                        _float rotRoll = rot["Roll"];

        //                        _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

        //                        vRotation.m128_f32[2] *= -1.f;

        //                        _float fX = XMVectorGetX(vRotation);
        //                        _float fZ = XMVectorGetZ(vRotation);

        //                        vRotation.m128_f32[0] = fZ;
        //                        vRotation.m128_f32[2] = fX;

        //                        vRotationOut = vRotation;
        //                    }

        //                    _vector vScale = {};
        //                    // 스케일
        //                    if (parentObject.contains("RelativeScale3D")) {
        //                        auto scale = parentObject["RelativeScale3D"];
        //                        _float fScaleX = scale.contains("X");
        //                        _float fScaleY = scale.contains("Y");
        //                        _float fScaleZ = scale.contains("Z");

        //                        fScaleX = SafeScaleToOne(fScaleX);
        //                        fScaleY = SafeScaleToOne(fScaleY);
        //                        fScaleZ = SafeScaleToOne(fScaleZ);

        //                        vScale = XMVectorSet(fScaleX, fScaleY, fScaleZ, 0.f);
        //                    }
        //                    vScale.m128_f32[0] = SafeScaleToOne(vScale.m128_f32[0]); // 현재 스케일 값이 1일경우 Json에 값을 저장하지 않으므로 scale이 0일 경우 0.01(m -> cm)로 바꿔줘야함
        //                    vScale.m128_f32[1] = SafeScaleToOne(vScale.m128_f32[1]);
        //                    vScale.m128_f32[2] = SafeScaleToOne(vScale.m128_f32[2]);


        //                    _matrix matRotation = XMMatrixRotationQuaternion(vRotationOut);

        //                    _matrix matScale = XMMatrixScaling(XMVectorGetX(vScale),
        //                        XMVectorGetY(vScale),
        //                        XMVectorGetZ(vScale));

        //                    _matrix matPosition = XMMatrixTranslation(XMVectorGetX(vLocation), XMVectorGetY(vLocation), XMVectorGetZ(vLocation));

        //                    matWorld = matScale * matRotation * matPosition;
        //                }

        //                //메쉬 이름 구해오기(보통 Blueprint나 부모에 저장되어 있음)

        //                if (iter.contains("Properties"))
        //                {
        //                    const auto& ChildName = iter["Properties"];

        //                    if (ChildName.contains("StaticMesh"))
        //                    {
        //                        string strMeshName = GetFileNameFromPath(ChildName["StaticMesh"]["ObjectName"]);

        //                        parentObjectNames.push_back(strMeshName);
        //                    }
        //                    else if (iter.contains("Template"))
        //                    {
        //                        string ParentName = GetFileNameFromPath(iter["Template"]["ObjectPath"]);
        //                        string BPName = StripNumberSuffix(ParentName);

        //                        string strVariableName = ExtractVariableName(obj["Template"]["ObjectName"]);

        //                        string strParentFinalPath = "../Bin/DataFiles/" + BPName + ".json";

        //                        std::ifstream parentfile(strParentFinalPath);
        //                        if (!parentfile.is_open()) {
        //                            std::cerr << "파일을 열 수 없습니다.\n";
        //                            continue;
        //                            //return E_FAIL;
        //                        }

        //                        json Parentj;

        //                        parentfile >> Parentj;


        //                        for (const auto& parentobj : Parentj)
        //                        {
        //                            if (parentobj.contains("Type") && parentobj["Type"] == "HierarchicalInstancedStaticMeshComponent") {

        //                                if (parentobj["Name"] != strVariableName)   continue;

        //                                const auto& parentprops = parentobj["Properties"];

        //                                if (parentprops.contains("StaticMesh"))
        //                                {
        //                                    const auto& NameProp = parentprops["StaticMesh"];

        //                                    if (NameProp.contains("ObjectName"))
        //                                        parentObjectNames.push_back(ExtractQuotedText(parentprops["StaticMesh"]["ObjectName"]));
        //                                }
        //                            }

        //                            if (parentobj.contains("Type") && parentobj["Type"] == BPName + "_C")
        //                            {
        //                                const auto& BPProps = parentobj["Properties"];

        //                                _int iRow = { 0 };
        //                                _int iColumn = { 0 };

        //                                if (BPProps.contains("Rows"))
        //                                    iRow = BPProps["Rows"];

        //                                if (BPProps.contains("Columns"))
        //                                    iColumn = BPProps["Columns"];

        //                                _float3 LocationOffset = {};

        //                                if (BPProps.contains("LocationOffset"))
        //                                {
        //                                    auto LOffset = BPProps["LocationOffset"];

        //                                    LocationOffset = _float3(LOffset["X"] * 0.001f, LOffset["Y"] * -0.001f, LOffset["Z"] * 0.001f);
        //                                }

        //                                _float3 ScaleBase = {};
        //                                if (BPProps.contains("ScaleBase"))
        //                                {
        //                                    auto LScaleBase = BPProps["ScaleBase"];

        //                                    ScaleBase = _float3(LScaleBase["X"], LScaleBase["Z"], LScaleBase["Y"]);
        //                                }

        //                                _vector RotationBase = {};
        //                                if (BPProps.contains("RotationBase"))
        //                                {
        //                                    auto LRotationBase = BPProps["RotationBase"];

        //                                    _float rotPitch = LRotationBase["Pitch"];
        //                                    _float rotYaw = LRotationBase["Yaw"];
        //                                    _float rotRoll = LRotationBase["Roll"];

        //                                    _vector vRotation = {};
        //                                    if (rotPitch <= 0.0001f && rotYaw <= 0.0001f && rotRoll <= 0.0001f)
        //                                    {
        //                                        vRotation = XMVectorSet(0.f, 0.f, 0.f, 1.f);
        //                                    }
        //                                    else
        //                                    {
        //                                        _vector vRot = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

        //                                        vRot.m128_f32[2] *= -1.f;

        //                                        _vector vRotationY = XMQuaternionRotationAxis(XMVectorSet(0.f, 0.f, 1.f, 0.f), XMConvertToRadians(-90.f));

        //                                        _vector vRotationX = XMQuaternionRotationAxis(XMVectorSet(1.f, 0.f, 0.f, 0.f), XMConvertToRadians(-90.f));

        //                                        _vector vRotationFinal = XMQuaternionMultiply(vRotationX, vRotationY);

        //                                        vRotation = XMQuaternionMultiply(vRot, vRotationY);
        //                                    }


        //                                    RotationBase = vRotation;
        //                                }

        //                                _int centerRow = iRow / 2;
        //                                _int centerColumn = iColumn / 2;

        //                                _matrix matTranslation = XMMatrixIdentity();
        //                                _matrix matRotation = XMMatrixRotationQuaternion(RotationBase);
        //                                _matrix matScale = XMMatrixScaling(ScaleBase.x, ScaleBase.y, ScaleBase.z);

        //                                parentBluePrintedMatrix.push_back(matScale * matRotation * matTranslation);

        //                            }
        //                        }
        //                    }
        //                }

        //            }
        //        }


        //    }

        //    if (!obj.contains("PerInstanceSMData"))continue;


        //    for (const auto& instance : obj["PerInstanceSMData"]) {
        //        if (instance.contains("TransformData")) {
        //            const auto& transform = instance["TransformData"];

        //            // 접근 예시
        //            MESHINFO_DESC info;

        //            _matrix matLocal = {};

        //            if (parentObjectNames.size() > 0)
        //            {
        //                info.ObjectName = parentObjectNames[0];

        //                _vector vLocalTranslation = {};
        //                if (transform.contains("Translation"))
        //                {
        //                    auto translation = transform["Translation"];

        //                    _float fPositionX = translation["X"] * 0.01f;
        //                    _float fPositionY = translation["Y"] * -0.01f;
        //                    _float fPositionZ = translation["Z"] * 0.01f;

        //                    vLocalTranslation = XMVectorSet(fPositionX, fPositionZ, fPositionY, 0.f);
        //                }

        //                _vector vLocalRotation = {};
        //                if (transform.contains("Rotation")) {
        //                    auto rotation = transform["Rotation"];
        //                    _float rotX = rotation["X"];
        //                    _float rotY = rotation["Y"];
        //                    _float rotZ = rotation["Z"];
        //                    _float rotW = rotation["W"];

        //                    _vector vRotation = XMVectorSet(rotX, rotY, rotZ, rotW);

        //                    vLocalRotation = vRotation;
        //                }
        //                _vector vLocalScale = {};
        //                if (transform.contains("Scale3D")) {
        //                    auto scale = transform["Scale3D"];
        //                    _float fScaleX = scale.contains("X") ? SafeScale(scale["X"].get<float>()) : 1.0f;
        //                    _float fScaleY = scale.contains("Y") ? SafeScale(scale["Y"].get<float>()) : 1.0f;
        //                    _float fScaleZ = scale.contains("Z") ? SafeScale(scale["Z"].get<float>()) : 1.0f;

        //                    fScaleX *= 0.01f;
        //                    fScaleY *= 0.01f;
        //                    fScaleZ *= 0.01f;

        //                    fScaleX = SafeScale(fScaleX);
        //                    fScaleY = SafeScale(fScaleY);
        //                    fScaleZ = SafeScale(fScaleZ);


        //                    vLocalScale = XMVectorSet(fScaleX, fScaleZ, fScaleY, 0.f);
        //                }

        //                vLocalScale.m128_f32[0] = SafeScale(vLocalScale.m128_f32[0]);
        //                vLocalScale.m128_f32[1] = SafeScale(vLocalScale.m128_f32[1]);
        //                vLocalScale.m128_f32[2] = SafeScale(vLocalScale.m128_f32[2]);

        //                _matrix matLocalRotation = XMMatrixRotationQuaternion(vLocalRotation);

        //                _matrix matLocalScale = XMMatrixScaling(XMVectorGetX(vLocalScale),
        //                    XMVectorGetY(vLocalScale),
        //                    XMVectorGetZ(vLocalScale));

        //                _matrix matLocalPosition = XMMatrixTranslation(XMVectorGetX(vLocalTranslation), XMVectorGetY(vLocalTranslation), XMVectorGetZ(vLocalTranslation));

        //                matLocal = matLocalScale * matLocalRotation * matLocalPosition;

        //                _matrix matOut = matWorld; // matWorld바뀌지 않도록하기

        //                if (parentBluePrintedMatrix.size() > 0)
        //                {
        //                    _matrix matDesc = parentBluePrintedMatrix.front() * matLocal * matOut;

        //                    _vector vOutPos, vOutRot, vOutScale;

        //                    XMMatrixDecompose(&vOutScale, &vOutRot, &vOutPos, matDesc);

        //                    _float fY = vOutScale.m128_f32[1];
        //                    _float fZ = vOutScale.m128_f32[2];

        //                    vOutScale.m128_f32[2] = fZ;
        //                    vOutScale.m128_f32[1] = fY;

        //                    XMStoreFloat3(&info.fPosition, vOutPos);
        //                    XMStoreFloat3(&info.fScale, vOutScale);
        //                    XMStoreFloat4(&info.fRotation, vOutRot);

        //                    results.push_back(info);
        //                }
        //                else
        //                {
        //                    _matrix matDesc = matLocal * matOut;

        //                    _vector vOutPos, vOutRot, vOutScale;

        //                    XMMatrixDecompose(&vOutScale, &vOutRot, &vOutPos, matDesc);

        //                    XMStoreFloat3(&info.fPosition, vOutPos);
        //                    XMStoreFloat3(&info.fScale, vOutScale);
        //                    XMStoreFloat4(&info.fRotation, vOutRot);

        //                    results.push_back(info);
        //                }
        //            }
        //        }
        //    }
        //}
   // }

    // 결과 출력
    for (auto& mesh : results) {
        std::cout << " Mesh: " << mesh.ObjectName << "\n";
        std::cout << "    Location: (" << mesh.fPosition.x << ", " << mesh.fPosition.y << ", " << mesh.fPosition.z << ")\n";
        std::cout << "    Rotation: (" << mesh.fRotation.x << ", " << mesh.fRotation.y << ", " << mesh.fRotation.z << ", " << mesh.fRotation.w << ")\n";
        std::cout << "    Scale: (" << mesh.fScale.x << ", " << mesh.fScale.y << ", " << mesh.fScale.z << ")\n\n";
    }

    return S_OK;
}


HRESULT CJsonExtractor::FindMeshInfofromJson(const _char* pFilePath) // 메쉬 이름과 transform만 가져오는 함수
{
    std::ifstream file(pFilePath);
    if (!file.is_open()) {
        std::cerr << "파일을 열 수 없습니다.\n";
        return E_FAIL;
    }
    json j;
    file >> j;

    for (const auto& obj : j) {
        if (obj.contains("Type") && obj["Type"] == "StaticMeshComponent") {
            const auto& props = obj["Properties"];
            if (!props.contains("StaticMesh")) continue;

            MESHINFO_DESC info;
            string strFinalPath = GetFileNameFromPath(props["StaticMesh"]["ObjectPath"]);
            strFinalPath = StripNumberSuffix(strFinalPath);
            info.ObjectName = strFinalPath;

            // 위치
            if (props.contains("RelativeLocation")) {
                auto loc = props["RelativeLocation"];
                _float fPositionX = loc["X"];
                _float fPositionY = loc["Y"];
                _float fPositionZ = loc["Z"] * -1.f;

                XMStoreFloat3(&info.fPosition, XMVectorSet(fPositionX, fPositionY, fPositionZ, 0.f));
            }

            // 회전
            if (props.contains("Rotation")) {
                auto rot = props["Rotation"];
                _float rotPitch = rot["Pitch"];
                _float rotYaw = rot["Yaw"];
                _float rotRoll = rot["Roll"];

                _vector vRotation = XMQuaternionRotationRollPitchYaw(rotPitch, rotYaw, rotRoll);

                vRotation.m128_f32[2] *= -1.f;

                XMVECTOR vAdditionalRotation = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(180.0f));

                XMStoreFloat4(&info.fRotation, vAdditionalRotation);
            }

            // 스케일
            if (props.contains("RelativeScale3D")) {
                auto scale = props["RelativeScale3D"];
                _float fScaleX = scale.contains("X") ? SafeScale(scale["X"].get<float>()) : 1.0f;
                _float fScaleY = scale.contains("Y") ? SafeScale(scale["Y"].get<float>()) : 1.0f;
                _float fScaleZ = scale.contains("Z") ? SafeScale(scale["Z"].get<float>()) : 1.0f;

                fScaleX *= 0.01f;
                fScaleY *= 0.01f;
                fScaleZ *= 0.01f;

                fScaleX = SafeScale(fScaleX);
                fScaleY = SafeScale(fScaleY);
                fScaleZ = SafeScale(fScaleZ);

                XMStoreFloat3(&info.fScale, XMVectorSet(fScaleX, fScaleY, fScaleZ, 0.f));
            }
            if (props.contains("Scale3D")) {
                auto scale = props["Scale3D"];
                _float fScaleX = scale.contains("X") ? SafeScale(scale["X"].get<float>()) : 1.0f;
                _float fScaleY = scale.contains("Y") ? SafeScale(scale["Y"].get<float>()) : 1.0f;
                _float fScaleZ = scale.contains("Z") ? SafeScale(scale["Z"].get<float>()) : 1.0f;

                fScaleX *= 0.01f;
                fScaleY *= 0.01f;
                fScaleZ *= 0.01f;

                fScaleX = SafeScale(fScaleX);
                fScaleY = SafeScale(fScaleY);
                fScaleZ = SafeScale(fScaleZ);


                XMStoreFloat3(&info.fScale, XMVectorSet(fScaleX, fScaleY, fScaleZ, 0.f));
            }

            results.push_back(info);
        }
    }
    // 결과 출력
    for (auto& mesh : results) {
        std::cout << " Mesh: " << mesh.ObjectName << "\n";
        std::cout << "    Location: (" << mesh.fPosition.x << ", " << mesh.fPosition.y << ", " << mesh.fPosition.z << ")\n";
        std::cout << "    Rotation: (" << mesh.fRotation.x << ", " << mesh.fRotation.y << ", " << mesh.fRotation.z << ", " << mesh.fRotation.w << ")\n";
        std::cout << "    Scale: (" << mesh.fScale.x << ", " << mesh.fScale.y << ", " << mesh.fScale.z << ")\n\n";
    }

    return S_OK;
}

HRESULT CJsonExtractor::Export_MeshName(const _char* pFilePath) // 메모장에 메쉬이름들만 빼기
{
    if (results.size() == 0)
        return E_FAIL;

    vector<string> fileNames;

    for (auto& pMeshes : results)
    {
        string strFinalPath = GetFileNameFromPath(pMeshes.ObjectName);
        strFinalPath = StripNumberSuffix(strFinalPath);
        fileNames.push_back(strFinalPath);
    }

    std::sort(fileNames.begin(), fileNames.end());
    fileNames.erase(std::unique(fileNames.begin(), fileNames.end()), fileNames.end());

    std::ofstream outFile(pFilePath);
    if (!outFile.is_open())
        return E_FAIL;

    for (const auto& name : fileNames) {
        outFile << "\"" << name << "\",\n";
    }



    return S_OK;
}


HRESULT CJsonExtractor::Export_MeshPath(const _char* pFilePath)// 메모장에 메쉬경로들만 빼기
{
    if (results.size() == 0)
        return E_FAIL;

    vector<string> fileNames;

    for (auto& pMeshes : results)
    {
        fileNames.push_back(pMeshes.ObjectName);
    }

    std::sort(fileNames.begin(), fileNames.end());
    fileNames.erase(std::unique(fileNames.begin(), fileNames.end()), fileNames.end());

    std::ofstream outFile(pFilePath);
    if (!outFile.is_open())
        return E_FAIL;

    for (const auto& name : fileNames) {
        outFile << "\"" << name << "\",\n";
    }

    return S_OK;
}

HRESULT CJsonExtractor::Get_Infos(vector<MESHINFO_DESC>& pObjectInfo) // Desc에 쓸 정보 가져오기
{
    if (results.size() == 0)
        return E_FAIL;

    for (auto& pMeshes : results)
    {
        string strFinalPath = GetFileNameFromPath(pMeshes.ObjectName);
        strFinalPath = StripNumberSuffix(strFinalPath);

        MESHINFO_DESC desc = {};
        desc.ObjectName = strFinalPath;
        desc.fPosition = pMeshes.fPosition;
        desc.fRotation = pMeshes.fRotation;
        desc.fScale = pMeshes.fScale;

        pObjectInfo.push_back(desc);
    }

    return S_OK;
}

void CJsonExtractor::Find_StaticMeshes(unordered_map<int, json> mapIndex, json j)
{
    regex indexRegex(R"(\.([0-9]+)$)"); // 인덱스 찾기용 변수들
    smatch match2;

    const auto& props = j["Properties"];
    if (!props.contains("StaticMesh")) 
        return;

    MESHINFO_DESC info;
    string strFinalPath = GetFileNameFromPath(props["StaticMesh"]["ObjectPath"]); // 경로 가져오기 (ObjectPath대신 ObjectName부르면 메쉬이름만 가져와짐)
    strFinalPath = StripNumberSuffix(strFinalPath);
    info.ObjectName = strFinalPath;

    _matrix matWorld = XMMatrixIdentity();

    if (props.contains("AttachParent")) // 부모 찾기 현재 메쉬에 부모 블루프린트 Transform에 곱하는 형식이 아니면 쓸모 X
    {
        string parentName = ExtractSimpleName(j["Properties"]["AttachParent"]["ObjectPath"]);

        if (std::regex_search(parentName, match2, indexRegex)) { // 부모 인덱스로 찾아오기
            int index = std::stoi(match2[1].str());

            const auto& iter = mapIndex[index];

            if (iter.contains("Properties"))
            {
                const auto& parentObject = iter["Properties"];

                _vector vLocation = {};
                if (parentObject.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
                    auto loc = parentObject["RelativeLocation"];
                    vLocation.m128_f32[0] = loc["X"] * 0.01f;
                    vLocation.m128_f32[1] = loc["Z"] * 0.01f;
                    vLocation.m128_f32[2] = loc["Y"] * -0.01f;
                    vLocation.m128_f32[3] = 1.f;
                }

                _vector vRotationOut = {};
                if (parentObject.contains("RelativeRotation")) { // 회전 따로 Z-UP to Y-UP 적용하지 않음
                    auto rot = parentObject["RelativeRotation"];
                    _float rotPitch = rot["Pitch"];
                    _float rotYaw = rot["Yaw"];
                    _float rotRoll = rot["Roll"];

                    _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

                    vRotationOut = vRotation;
                }

                _vector vScale = {};
                // 스케일
                if (parentObject.contains("RelativeScale3D")) { // 크기 보통 0으로 되있으므로 1로 설정
                    auto scale = parentObject["RelativeScale3D"];
                    _float fScaleX = scale.contains("X") ? SafeScale(scale["X"].get<float>()) : 1.0f;
                    _float fScaleY = scale.contains("Y") ? SafeScale(scale["Y"].get<float>()) : 1.0f;
                    _float fScaleZ = scale.contains("Z") ? SafeScale(scale["Z"].get<float>()) : 1.0f;

                    fScaleX = SafeScale(fScaleX);
                    fScaleY = SafeScale(fScaleY);
                    fScaleZ = SafeScale(fScaleZ);

                    vScale = XMVectorSet(fScaleX, fScaleY, fScaleZ, 0.f);
                }
                vScale.m128_f32[0] = 1.f;
                vScale.m128_f32[1] = 1.f;
                vScale.m128_f32[2] = 1.f;


                _matrix matRotation = XMMatrixRotationQuaternion(vRotationOut);

                _matrix matScale = XMMatrixScaling(XMVectorGetX(vScale),
                    XMVectorGetY(vScale),
                    XMVectorGetZ(vScale));

                _matrix matPosition = XMMatrixTranslation(XMVectorGetX(vLocation), XMVectorGetY(vLocation), XMVectorGetZ(vLocation));

                matWorld = matScale * matRotation * matPosition;
            }
        }



    }
    // 위치

    _vector vLocalPosition = {};
    if (props.contains("RelativeLocation")) { // 보통 Relative는 부모의 Transform과 서로 곱하여 정해지지만 Parent가 없거나 parent에 따로 Transform이없는경우 RelativeLocation이 현재 위치가 됨
        auto loc = props["RelativeLocation"];
        _float fPositionX = loc["X"] * 0.01f;
        _float fPositionY = loc["Y"] * -0.01f;
        _float fPositionZ = loc["Z"] * 0.01f;

        vLocalPosition = XMVectorSet(fPositionX, fPositionZ, fPositionY, 0.f);
    }

    // 회전 (절대!!! 건들지 말것)
    _vector vLocalRotation = {};
    if (props.contains("RelativeRotation")) {
        auto rot = props["RelativeRotation"];
        _float rotPitch = rot["Pitch"];
        _float rotYaw = rot["Yaw"];
        _float rotRoll = rot["Roll"];

        _vector vRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotPitch), XMConvertToRadians(rotYaw), XMConvertToRadians(rotRoll));

        vRotation.m128_f32[2] *= -1.f;

        XMVECTOR vAdditionalRotation = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(180.0f));


        XMVECTOR vNewRotation = XMQuaternionMultiply(vRotation, vAdditionalRotation);

        _float fX = XMVectorGetX(vNewRotation); // 원래 Z축으로 90도 회전을 해야하는데 XMQuaternionMultiply로 Z축 90도 회전하면 값이 이상해짐..
        _float fZ = XMVectorGetZ(vNewRotation); // X와 Z값 서로 바꿔주기

        vNewRotation.m128_f32[0] = fZ;
        vNewRotation.m128_f32[2] = fX;

        vLocalRotation = vNewRotation;

    }

    // 스케일
    _vector vLocalScale = {};
    if (props.contains("RelativeScale3D")) {
        auto scale = props["RelativeScale3D"];
        _float fScaleX = scale.contains("X") ? SafeScale(scale["X"].get<float>()) : 1.0f;
        _float fScaleY = scale.contains("Y") ? SafeScale(scale["Y"].get<float>()) : 1.0f;
        _float fScaleZ = scale.contains("Z") ? SafeScale(scale["Z"].get<float>()) : 1.0f;

        fScaleX *= 0.01f;
        fScaleY *= 0.01f;
        fScaleZ *= 0.01f;

        fScaleX = SafeScale(fScaleX);
        fScaleY = SafeScale(fScaleY);
        fScaleZ = SafeScale(fScaleZ);


        vLocalScale = XMVectorSet(fScaleX, fScaleZ, fScaleY, 0.f); // Z-Up기준 sclae값 Y-Up으로 변경
    }

    vLocalScale.m128_f32[0] = SafeScale(vLocalScale.m128_f32[0]); // 현재 스케일 값이 1일경우 Json에 값을 저장하지 않으므로 scale이 0일 경우 0.01(m -> cm)로 바꿔줘야함
    vLocalScale.m128_f32[1] = SafeScale(vLocalScale.m128_f32[1]);
    vLocalScale.m128_f32[2] = SafeScale(vLocalScale.m128_f32[2]);

    _matrix matLocalRotation = XMMatrixRotationQuaternion(vLocalRotation);

    _matrix matLocalScale = XMMatrixScaling(XMVectorGetX(vLocalScale),
        XMVectorGetY(vLocalScale),
        XMVectorGetZ(vLocalScale));

    _matrix matLocalPosition = XMMatrixTranslation(XMVectorGetX(vLocalPosition), XMVectorGetY(vLocalPosition), XMVectorGetZ(vLocalPosition));

    _matrix matLocal = matLocalScale * matLocalRotation * matLocalPosition;

    matWorld = matLocal * matWorld; // 월드 위치와 로컬 위치 곱 (월드는 기본적으로 identity라 부모가 없을경우 그대로 출력

    _vector vOutPos, vOutRot, vOutScale;

    XMMatrixDecompose(&vOutScale, &vOutRot, &vOutPos, matWorld); // 로컬 * 월드한 transform

    XMStoreFloat3(&info.fPosition, vOutPos);
    XMStoreFloat3(&info.fScale, vOutScale);
    XMStoreFloat4(&info.fRotation, vOutRot);

    results.push_back(info);
}

CJsonExtractor* CJsonExtractor::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CJsonExtractor* pInstance = new CJsonExtractor(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed To Created : CJsonExtractor");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CComponent* CJsonExtractor::Clone(void* pArg)
{
	CJsonExtractor* pInstance = new CJsonExtractor(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed To Cloned : CJsonExtractor");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CJsonExtractor::Free()
{
	__super::Free();

}
