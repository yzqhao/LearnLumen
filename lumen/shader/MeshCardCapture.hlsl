cbuffer GlobalConstants{
    float4x4 mProjectionMatrix;
    float4x4 mViewMatrix;
    float4x4 mWorldToClipMatrix;
    float4 mCameraPositionHighWS;
    float4 mCameraPositionLowWS;
    float4 mViewDirectionWS;
    float4 mViewRightWS;
    float4 mViewUpWS;
    float4x4 mModelMatrices[2];//scale translate
    float4x4 mITModelMatrices[2];
    float4x4 mScreenToTranslatedWorld;
    float4 mCameraPositionMeshCardCapture[12];
    float4x4 mViewMatrixMeshCardCapture[6];//+x,-x,... axis
    float4x4 mProjectionMatrixMeshCardCapture[2];
}

cbuffer RootConstant : register(b1)
{
    uint4 mMisc;
};

void VS(in float3 inPositionMS: POSITION,
    in float4 inTangentX:TANGENTX,
    in float4 inTangentZ:TANGENTZ,
    out float4 outPosition:SV_POSITION,
    out float3 outNormal:NORMAL){
    uint cameraPositionIndex=mMisc.x;
    uint viewMatrixIndex=mMisc.y;
    uint projectionMatrixIndex=mMisc.z;
    uint modelMatrixIndex=mMisc.w;
    float4 positionWS=mul(float4(inPositionMS,1.0f),mModelMatrices[modelMatrixIndex]);
    positionWS=float4(positionWS.xyz-mCameraPositionMeshCardCapture[cameraPositionIndex].xyz,1.0f);//translated world
    float4 positionVS=mul(positionWS,mViewMatrixMeshCardCapture[viewMatrixIndex]);
    float4 positionCS=mul(positionVS,mProjectionMatrixMeshCardCapture[projectionMatrixIndex]);
    float3 normal=mul(float4(inTangentZ.xyz,0.0f),mITModelMatrices[modelMatrixIndex]).xyz;
    outNormal=normal;
    outPosition = positionCS;
}
//(0.0f,0.0f,1.0f) => (0.3,0.4,0.4) => 0.3*0.3+0.4*0.4+0.4*0.4 != 1.0
void PS(
    in float4 inPosition:SV_POSITION,
    in float3 inNormal:NORMAL,
    out float4 outRT0:SV_TARGET0,//albedo
    out float4 outRT1:SV_TARGET1,//normal
    out float4 outRT2:SV_TARGET2){//emissive
    uint viewMatrixIndex=mMisc.y;
    float3 normalWS=normalize(inNormal);
    float3 card_space_normal=mul(float4(normalWS,0.0f),mViewMatrixMeshCardCapture[viewMatrixIndex]).xyz;
    float3 diffuseColor=float3(0.9f,0.9f,0.9f);
    float3 specularColor=0.08f*float3(0.5f,0.5f,0.5f);
    diffuseColor += specularColor * 0.45;

    outRT0=float4(sqrt(diffuseColor),1.0f);
    outRT1=float4(card_space_normal.xy*0.5f+0.5f,0.0f,1.0f);
    outRT2=float4(0.0f,0.0f,0.0f,0.0f);
}