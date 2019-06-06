/**
   @author Shin'ichiro Nakaoka
*/

#ifndef CNOID_BASE_SHADER_PROGRAMS_H
#define CNOID_BASE_SHADER_PROGRAMS_H

#include "glcore.h"
#include <cnoid/EigenTypes>
#include "exportdecl.h"

namespace cnoid {

class GLSLProgram;
class SgCamera;
class SgLight;
class SgMaterial;
class SgFog;

class CNOID_EXPORT ShaderProgram
{
    GLSLProgram* glslProgram_;
    
protected:
    ShaderProgram();
    ShaderProgram(const ShaderProgram&) = delete;

public:
    virtual ~ShaderProgram();
    GLSLProgram& glslProgram(){ return *glslProgram_; }
    virtual void initialize();
    virtual void activate();
    virtual void initializeFrameRendering();
    virtual void deactivate();
    virtual void setTransform(const Affine3& view, const Affine3& model, const Matrix4& PV);
    virtual void setMaterial(const SgMaterial* material);
};


class NolightingProgramImpl;

class CNOID_EXPORT NolightingProgram : public ShaderProgram
{
protected:
    NolightingProgram();
    NolightingProgram(const NolightingProgram&) = delete;
    ~NolightingProgram();
    
public:
    virtual void initialize() override;
    virtual void setTransform(const Affine3& view, const Affine3& model, const Matrix4& PV) override;

private:
    NolightingProgramImpl* impl;
};


class SolidColorProgramImpl;

class CNOID_EXPORT SolidColorProgram : public NolightingProgram
{
    SolidColorProgram(const SolidColorProgram&) = delete;
    
public:
    SolidColorProgram();
    ~SolidColorProgram();

    virtual void initialize() override;
    virtual void initializeFrameRendering() override;

    virtual void setMaterial(const SgMaterial* material) override;
    
    void setColor(const Vector3f& color);
    void enableColorArray(bool on);
    void setColorChangable(bool on);
    bool isColorChangable() const;
    void setPointSize(float s);

private:
    SolidColorProgramImpl* impl;
};


class CNOID_EXPORT LightingProgram : public ShaderProgram
{
protected:
    LightingProgram() = default;
    LightingProgram(const LightingProgram&) = delete;
    
public:
    virtual int maxNumLights() const = 0;
    virtual bool setLight(
        int index, const SgLight* light, const Affine3& T, const Affine3& view, bool shadowCasting) = 0;
    virtual void setNumLights(int n) = 0;
    virtual void setFog(const SgFog* fog);
};


class MinimumLightingProgramImpl;

class CNOID_EXPORT MinimumLightingProgram : public LightingProgram
{
    MinimumLightingProgram(const MinimumLightingProgram&) = delete;
    
public:
    MinimumLightingProgram();
    ~MinimumLightingProgram();

    virtual void initialize() override;
    virtual void activate() override;
    virtual void initializeFrameRendering() override;
    virtual void setTransform(const Affine3& view, const Affine3& model, const Matrix4& PV) override;
    virtual int maxNumLights() const override;
    virtual bool setLight(
        int index, const SgLight* light, const Affine3& T, const Affine3& view, bool shadowCasting) override;
    virtual void setNumLights(int n) override;
    virtual void setMaterial(const SgMaterial* material) override;

private:
    MinimumLightingProgramImpl* impl;
};


class BasicLightingProgramImpl;

class CNOID_EXPORT BasicLightingProgram : public LightingProgram
{
protected:
    BasicLightingProgram();
    BasicLightingProgram(const BasicLightingProgram&) = delete;
    ~BasicLightingProgram();
    
public:
    virtual void initialize() override;
    virtual int maxNumLights() const override;
    virtual bool setLight(
        int index, const SgLight* light, const Affine3& T, const Affine3& view, bool shadowCasting) override;
    virtual void setNumLights(int n) override;
    virtual void setFog(const SgFog* fog) override;

private:
    BasicLightingProgramImpl* impl;
};


class MaterialLightingProgramImpl;

class CNOID_EXPORT MaterialLightingProgram : public BasicLightingProgram
{
protected:
    MaterialLightingProgram();
    MaterialLightingProgram(const MaterialLightingProgram&) = delete;
    ~MaterialLightingProgram();
    
public:
    virtual void initialize() override;
    virtual void activate() override;
    virtual void setMaterial(const SgMaterial* material) override;
    void setTextureEnabled(bool on);
    void setVertexColorEnabled(bool on);

private:
    MaterialLightingProgramImpl* impl;
    friend class MaterialLightingProgramImpl;
};


class PhongShadowLightingProgramImpl;
class ShadowMapProgram;

class PhongShadowLightingProgram : public MaterialLightingProgram
{
    PhongShadowLightingProgram(const PhongShadowLightingProgram&) = delete;

public:
    PhongShadowLightingProgram();
    ~PhongShadowLightingProgram();

    void setDefaultFramebufferObject(GLuint id);
    GLuint defaultFramebufferObject() const;

    virtual void initialize() override;
    virtual void activate() override;
    virtual void initializeFrameRendering() override;
    virtual bool setLight(
        int index, const SgLight* light, const Affine3& T, const Affine3& view, bool shadowCasting) override;
    virtual void setTransform(const Affine3& view, const Affine3& model, const Matrix4& PV) override;

    void activateShadowMapGenerationPass(int shadowIndex);
    void activateMainRenderingPass();

    int maxNumShadows() const;
    void setNumShadows(int n);
    ShadowMapProgram& shadowMapProgram();
    void getShadowMapSize(int& width, int& height) const;
    SgCamera* getShadowMapCamera(SgLight* light, Affine3& io_T);
    void setShadowMapViewProjection(const Matrix4& PV);
    void setShadowAntiAliasingEnabled(bool on);
    bool isShadowAntiAliasingEnabled() const;

private:
    PhongShadowLightingProgramImpl* impl;
    friend class PhongShadowLightingProgramImpl;
    friend class ShadowMapProgram;
};


class ShadowMapProgram : public NolightingProgram
{
    PhongShadowLightingProgram* mainProgram;

    ShadowMapProgram(const ShadowMapProgram&) = delete;
    
public:
    ShadowMapProgram(PhongShadowLightingProgram* mainProgram);
    virtual void initialize() override;
    virtual void activate() override;
    virtual void initializeFrameRendering() override;
    virtual void deactivate() override;
};

}

#endif
