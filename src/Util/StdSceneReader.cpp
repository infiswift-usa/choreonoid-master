/**
   \file
   \author Shin'ichiro Nakaoka
*/

#include "StdSceneReader.h"
#include "SceneDrawables.h"
#include "SceneLights.h"
#include "MeshGenerator.h"
#include "PolygonMeshTriangulator.h"
#include "MeshFilter.h"
#include "SceneLoader.h"
#include "YAMLReader.h"
#include "EigenArchive.h"
#include "FilePathVariableProcessor.h"
#include "NullOut.h"
#include "ImageIO.h"
#include "UTF8.h"
#include <cnoid/stdx/filesystem>
#include <cnoid/Config>
#include <fmt/format.h>
#include <unordered_map>
#include <mutex>
#include <regex>
#include "gettext.h"

using namespace std;
using namespace cnoid;
using fmt::format;
namespace filesystem = stdx::filesystem;

namespace {

}

namespace cnoid {

class StdSceneReader::Impl
{
public:
    StdSceneReader* self;

    ostream* os_;
    ostream& os() { return *os_; }

    YAMLReader* mainYamlReader;

    // temporary variables for reading values
    double value;
    string symbol;
    Vector3f color;
    Vector3 v;
    Vector2 v2;
    bool on;
    
    MeshGenerator meshGenerator;
    PolygonMeshTriangulator polygonMeshTriangulator;
    MeshFilter meshFilter;
    SgMaterialPtr defaultMaterial;
    ImageIO imageIO;

    struct SceneNodeInfo
    {
        SgGroupPtr parent;
        SgNodePtr node;
        Matrix3 R;
        bool isScaled;
    };

    typedef unordered_map<string, SceneNodeInfo> SceneNodeMap;

    struct ResourceInfo : public Referenced
    {
        SgNodePtr scene;
        unique_ptr<SceneNodeMap> sceneNodeMap;
        unique_ptr<YAMLReader> yamlReader;
        string directory;
    };
    typedef ref_ptr<ResourceInfo> ResourceInfoPtr;

    map<string, ResourceInfoPtr> resourceInfoMap;
    
    SceneLoader sceneLoader;
    FilePathVariableProcessorPtr pathVariableProcessor;
    regex uriSchemeRegex;
    bool isUriSchemeRegexReady;
    typedef map<string, SgImagePtr> ImagePathToSgImageMap;
    ImagePathToSgImageMap imagePathToSgImageMap;
    bool generateTexCoord;

    typedef SgNode* (Impl::*NodeFunction)(Mapping& info);
    typedef unordered_map<string, NodeFunction> NodeFunctionMap;

    static NodeFunctionMap nodeFunctionMap;
    static std::mutex nodeFunctionMapMutex;
    static unordered_map<string, UriSchemeHandler> uriSchemeHandlerMap;
    static std::mutex uriSchemeHandlerMutex;

    Impl(StdSceneReader* self);
    ~Impl();

    bool readAngle(const Mapping& info, const char* key, double& angle) const {
        return self->readAngle(info, key, angle);
    }

    bool readAngle(const Mapping& info, std::initializer_list<const char*> keys, double& angle) const {
        bool found = false;
        for(auto& key : keys){
            found = self->readAngle(info, key, angle);
            if(found){
                break;
            }
        }
        return found;
    }

    FilePathVariableProcessor* getOrCreatePathVariableProcessor();
    SgNode* readNode(Mapping& info);
    SgNode* readNode(Mapping& info, const string& type);
    SgNode* readNodeNode(Mapping& info);
    SgNode* readGroup(Mapping& info);
    void readElements(Mapping& info, SgGroup* group);
    void readNodeList(ValueNode* elements, SgGroup* group);
    SgNode* readTransform(Mapping& info);
    SgNode* readTransformParameters(Mapping& info, SgNode* scene);
    SgNode* readShape(Mapping& info);
    SgMesh* readGeometry(Mapping& info);
    void readDivisionNumbers(Mapping& info, SgMesh* mesh);
    SgMesh* readBox(Mapping& info);
    SgMesh* readSphere(Mapping& info);
    SgMesh* readCylinder(Mapping& info);
    SgMesh* readCone(Mapping& info);
    SgMesh* readCapsule(Mapping& info);
    SgMesh* readExtrusion(Mapping& info);
    SgMesh* readElevationGrid(Mapping& info);
    SgMesh* readTriangleMesh(Mapping& info);
    SgMesh* readIndexedFaceSet(Mapping& info);
    SgMesh* readResourceAsGeometry(Mapping& info);
    void readAppearance(SgShape* shape, Mapping& info);
    void readMaterial(SgShape* shape, Mapping& info);
    void readTexture(SgShape* shape, Mapping& info);
    void readTextureTransform(SgTexture* texture, Mapping& info);
    void readLightCommon(Mapping& info, SgLight* light);
    SgNode* readDirectionalLight(Mapping& info);
    SgNode* readSpotLight(Mapping& info);
    SgNode* readResource(Mapping& info);
    Resource readResourceNode(Mapping& info);
    Resource loadResource(Mapping& resourceNode, const string& uri);
    void extractNamedYamlNodes(
        Mapping& resourceNode, ResourceInfo* info, vector<string>& names, const string& uri, Resource& resource);
    void extractNamedSceneNodes(
        Mapping& resourceNode, ResourceInfo* info, vector<string>& names, const string& uri, Resource& resource);
    void decoupleResourceNode(Mapping& resourceNode, const string& uri, const string& nodeName);
    ResourceInfo* getOrCreateResourceInfo(Mapping& resourceNode, const string& uri);
    stdx::filesystem::path findFileInPackage(const string& file);
    void adjustNodeCoordinate(SceneNodeInfo& info);
    void makeSceneNodeMap(ResourceInfo* info);
    void makeSceneNodeMapSub(const SceneNodeInfo& nodeInfo, SceneNodeMap& nodeMap);
};

StdSceneReader::Impl::NodeFunctionMap StdSceneReader::Impl::nodeFunctionMap;
std::mutex StdSceneReader::Impl::nodeFunctionMapMutex;
unordered_map<string, StdSceneReader::UriSchemeHandler> StdSceneReader::Impl::uriSchemeHandlerMap;
std::mutex StdSceneReader::Impl::uriSchemeHandlerMutex;

}

namespace {

template<typename ValueType>
bool extract(Mapping* mapping, const char* key, ValueType& out_value)
{
    ValueNodePtr node = mapping->extract(key);
    if(node){
        out_value = node->to<ValueType>();
        return true;
    }
    return false;
}

}


void StdSceneReader::registerUriSchemeHandler(const std::string& scheme, UriSchemeHandler handler)
{
    std::lock_guard<std::mutex> guard(Impl::uriSchemeHandlerMutex);
    Impl::uriSchemeHandlerMap[scheme] = handler;
}
                                                 

StdSceneReader::StdSceneReader()
{
    impl = new Impl(this);
    clear();
}


StdSceneReader::Impl::Impl(StdSceneReader* self)
    : self(self)
{
    {
        lock_guard<mutex> lock(nodeFunctionMapMutex);
        
        if(nodeFunctionMap.empty()){
            nodeFunctionMap = {
                { "Node",             &Impl::readNodeNode },
                { "Group",            &Impl::readGroup },
                { "Transform",        &Impl::readTransform },
                { "Shape",            &Impl::readShape },
                { "DirectionalLight", &Impl::readDirectionalLight },
                { "SpotLight",        &Impl::readSpotLight },
                { "Resource",         &Impl::readResource }
            };
        }
    }
    
    os_ = &nullout();
    isUriSchemeRegexReady = false;
    imageIO.setUpsideDown(true);
}


StdSceneReader::~StdSceneReader()
{
    delete impl;
}


StdSceneReader::Impl::~Impl()
{

}


void StdSceneReader::setMessageSink(std::ostream& os)
{
    impl->os_ = &os;
    impl->sceneLoader.setMessageSink(os);
}


void StdSceneReader::setDefaultDivisionNumber(int n)
{
    impl->meshGenerator.setDivisionNumber(n);
    impl->sceneLoader.setDefaultDivisionNumber(n);
}


int StdSceneReader::defaultDivisionNumber() const
{
    return impl->meshGenerator.divisionNumber();
}


void StdSceneReader::setBaseDirectory(const std::string& directory)
{
    impl->getOrCreatePathVariableProcessor()->setBaseDirectory(directory);
}


std::string StdSceneReader::baseDirectory() const
{
    if(impl->pathVariableProcessor){
        return impl->pathVariableProcessor->baseDirectory();
    }
    return string();
}


stdx::filesystem::path StdSceneReader::baseDirPath() const
{
    if(impl->pathVariableProcessor){
        return impl->pathVariableProcessor->baseDirPath();
    }
    return stdx::filesystem::path();
}


void StdSceneReader::setFilePathVariableProcessor(FilePathVariableProcessor* processor)
{
    impl->pathVariableProcessor = processor;
}


FilePathVariableProcessor* StdSceneReader::Impl::getOrCreatePathVariableProcessor()
{
    if(!pathVariableProcessor){
        pathVariableProcessor = new FilePathVariableProcessor;
    }
    return pathVariableProcessor;
}


void StdSceneReader::setYAMLReader(YAMLReader* reader)
{
    impl->mainYamlReader = reader;
}


void StdSceneReader::clear()
{
    isDegreeMode_ = true;
    impl->defaultMaterial.reset();
    impl->resourceInfoMap.clear();
    impl->imagePathToSgImageMap.clear();
}


void StdSceneReader::readHeader(Mapping& info)
{
    auto angleUnitNode = info.extract("angle_unit");
    if(!angleUnitNode){
        angleUnitNode = info.extract("angleUnit");
    }
    if(angleUnitNode){
        string unit = angleUnitNode->toString();
        if(unit == "radian"){
            setAngleUnit(RADIAN);
        } else if(unit == "degree"){
            setAngleUnit(DEGREE);
        } else {
            angleUnitNode->throwException(_("The \"angleUnit\" value must be either \"radian\" or \"degree\""));
        }
    }
}


void StdSceneReader::setAngleUnit(AngleUnit unit)
{
    isDegreeMode_ = (unit == DEGREE);
}


bool StdSceneReader::readAngle(const Mapping& info, const char* key, double& angle) const
{
    if(info.read(key, angle)){
        angle = toRadian(angle);
        return true;
    }
    return false;
}


bool StdSceneReader::readAngle(const Mapping& info, const char* key, float& angle) const
{
    if(info.read(key, angle)){
        angle = toRadian(angle);
        return true;
    }
    return false;
}


AngleAxis StdSceneReader::readAngleAxis(const Listing& rotation) const
{
    Vector4 r;
    cnoid::readEx(rotation, r);
    Vector3 axis(r[0], r[1], r[2]);
    double size = axis.norm();
    if(size < 1.0e-6){
        rotation.throwException("Rotation axis is the zero vector");
    }
    axis /= size; // normalize
    return AngleAxis(toRadian(r[3]), axis);
}


bool StdSceneReader::readRotation(const ValueNode* info, Matrix3& out_R) const
{
    if(!info || !info->isValid()){
        return false;
    }
    const Listing& rotations = *info->toListing();
    if(!rotations.empty()){
        if(!rotations[0].isListing()){
            out_R = readAngleAxis(rotations);
        } else {
            out_R = Matrix3::Identity();
            for(int i=0; i < rotations.size(); ++i){
                out_R = out_R * readAngleAxis(*rotations[i].toListing());
            }
        }
    }
    return true;
}

        
bool StdSceneReader::readRotation(const Mapping& info, Matrix3& out_R) const
{
    return readRotation(info.find("rotation"), out_R);
}


bool StdSceneReader::readRotation(const Mapping& info, const char* key, Matrix3& out_R) const
{
    return readRotation(info.find(key), out_R);
}


bool StdSceneReader::extractRotation(Mapping& info, Matrix3& out_R) const
{
    ValueNodePtr value = info.extract("rotation");
    return readRotation(value, out_R);
}


bool StdSceneReader::readTranslation(const ValueNode* info, Vector3& out_p) const
{
    if(!info || !info->isValid()){
        return false;
    }
    const Listing& translations = *info->toListing();
    if(!translations.empty()){
        if(!translations[0].isListing()){
            readEx(translations, out_p);
        } else {
            out_p.setZero();
            Vector3 v;
            for(int i=0; i < translations.size(); ++i){
                if(readTranslation(&translations[i], v)){
                    out_p += v;
                }
            }
        }
    }
    return true;
}


bool StdSceneReader::readTranslation(const Mapping& info, Vector3& out_p) const
{
    return readTranslation(info.find("translation"), out_p);
}


bool StdSceneReader::readTranslation(const Mapping& info, const char* key, Vector3& out_p) const
{
    return readTranslation(info.find(key), out_p);
}


bool StdSceneReader::extractTranslation(Mapping& info, Vector3& out_p) const
{
    ValueNodePtr value = info.extract("translation");
    return readTranslation(value, out_p);
}


SgNode* StdSceneReader::readNode(Mapping& info)
{
    return info.isValid() ? impl->readNode(info) : nullptr;
}


SgNode* StdSceneReader::readNode(Mapping& info, const std::string& type)
{
    return info.isValid() ? impl->readNode(info, type) : nullptr;
}


static SgNodePtr removeRedundantGroup(SgGroupPtr& group)
{
    SgNodePtr node;
    if(!group->empty()){
        if(group->numChildren() == 1 && typeid(group) == typeid(SgGroup)){
            node = group->child(0);
        } else {
            node = group;
        }
    }
    group.reset();
    return node;
}


SgNode* StdSceneReader::readScene(ValueNode* scene)
{
    SgGroupPtr group = new SgGroup;
    impl->readNodeList(scene, group);
    return removeRedundantGroup(group).retn();
}    


SgNode* StdSceneReader::Impl::readNode(Mapping& info)
{
    const string type = info["type"].toString();
    return readNode(info, type);
}
    

SgNode* StdSceneReader::Impl::readNode(Mapping& info, const string& type)
{
    NodeFunctionMap::iterator q = nodeFunctionMap.find(type);
    if(q == nodeFunctionMap.end()){
        if(info.get({ "is_optional", "isOptional" }, false)){
            os() << format(_("Warning: the node type \"{}\" is not defined. Reading this node has been skipped."), type) << endl;
            return nullptr;
        }
        info.throwException(format(_("The node type \"{}\" is not defined."), type));
    }

    NodeFunction funcToReadNode = q->second;
    SgNodePtr scene = (this->*funcToReadNode)(info);
    if(scene){
        if(info.read("name", symbol)){
            scene->setName(symbol);
        } else {
            // remove a nameless, redundant group node
            if(SgGroupPtr group = dynamic_pointer_cast<SgGroup>(scene)){
                scene = removeRedundantGroup(group);
            }
        }
    }
    return scene.retn();
}


SgNode* StdSceneReader::Impl::readNodeNode(Mapping& info)
{
    SgNodePtr node = new SgNode;
    return node.retn();
}


SgNode* StdSceneReader::Impl::readGroup(Mapping& info)
{
    SgGroupPtr group = new SgGroup;
    readElements(info, group);
    return group.retn();
}


void StdSceneReader::Impl::readElements(Mapping& info, SgGroup* group)
{
    auto elements = info.find("elements");
    if(elements->isValid()){
        readNodeList(elements, group);
    }
}


void StdSceneReader::Impl::readNodeList(ValueNode* elements, SgGroup* group)
{
    if(elements->isListing()){
        Listing& listing = *elements->toListing();
        for(int i=0; i < listing.size(); ++i){
            Mapping& element = *listing[i].toMapping();
            const string type = element["type"].toString();
            SgNodePtr scene = readNode(element, type);
            if(scene){
                group->addChild(scene);
            }
        }
    } else if(elements->isMapping()){
        Mapping& mapping = *elements->toMapping();
        Mapping::iterator p = mapping.begin();
        while(p != mapping.end()){
            const string& type = p->first;
            Mapping& element = *p->second->toMapping();
            ValueNode* typeNode = element.find("type");
            if(typeNode->isValid()){
                string type2 = typeNode->toString();
                if(type2 != type){
                    element.throwException(
                        format(_("The node type \"{0}\" is different from the type \"{1}\" specified in the parent node"),
                                type2, type));
                }
            }
            SgNodePtr scene = readNode(element, type);
            if(scene){
                group->addChild(scene);
            }
            ++p;
        }
    } else {
        elements->throwException(
            format(_("A scalar value is not accepted")));
    }
}


SgNode* StdSceneReader::Impl::readTransform(Mapping& info)
{
    SgGroupPtr group;

    SgPosTransformPtr posTransform = new SgPosTransform;
    if(self->readTranslation(info, v)){
        posTransform->setTranslation(v);
        group = posTransform;
    }        
    Matrix3 R;
    if(self->readRotation(info, R)){
        posTransform->setRotation(R);
        if(!group){
            group = posTransform;
        }
    }

    if(!read(info, "scale", v)){
        if(!group){
            group = new SgGroup;
        }
        readElements(info, group);

    } else {
        SgScaleTransformPtr scale = new SgScaleTransform;
        scale->setScale(v);
        if(group){
            group->addChild(scale);
        } else {
            group = scale;
        }
        readElements(info, scale);
    }

    // Necessary to prevent group from being deleted when exiting the function
    posTransform.reset();
    
    return group.retn();
}


SgNode* StdSceneReader::Impl::readTransformParameters(Mapping& info, SgNode* scene)
{
    SgGroupPtr group;
    
    Matrix3 R;
    bool isRotated = self->readRotation(info, R);
    Vector3 v;
    bool isTranslated = self->readTranslation(info, v);
    if(isRotated || isTranslated){
        SgPosTransformPtr transform = new SgPosTransform;
        if(isRotated){
            transform->setRotation(R);
        }
        if(isTranslated){
            transform->setTranslation(v);
        }
        group = transform;
    }
    
    if(read(info, "scale", v)){
        SgScaleTransformPtr scale = new SgScaleTransform;
        scale->setScale(v);
        if(group){
            group->addChild(scale);
            scale->addChild(scene);
            scene = nullptr;
        } else {
            group = scale;
        }
    }

    if(group){
        if(scene){
            group->addChild(scene);
        }
        return group.retn();
    }
    return scene;
}


SgNode* StdSceneReader::Impl::readShape(Mapping& info)
{
    SgNode* scene = nullptr;

    SgShapePtr shape = new SgShape;
    
    Mapping& appearance = *info.findMapping("appearance");
    if(appearance.isValid()){
        readAppearance(shape, appearance);
        if(shape->texture()){
            generateTexCoord = true;
        }else{
            generateTexCoord = false;
        }
    }

    Mapping& geometry = *info.findMapping("geometry");
    if(geometry.isValid()){
        shape->setMesh(readGeometry(geometry));
    }

    scene = readTransformParameters(info, shape);

    if(scene == shape){
        return shape.retn();
    }

    return scene;
}


SgMesh* StdSceneReader::Impl::readGeometry(Mapping& info)
{
    SgMesh* mesh = nullptr;
    ValueNode& typeNode = info["type"];
    string type = typeNode.toString();
    if(type == "Box"){
        mesh = readBox(info);
    } else if(type == "Sphere"){
        mesh = readSphere(info);
    } else if(type == "Cylinder"){
        mesh = readCylinder(info);
    } else if(type == "Cone"){
        mesh = readCone(info);
    } else if(type == "Capsule"){
        mesh = readCapsule(info);
    } else if(type == "Extrusion"){
        mesh = readExtrusion(info);
    } else if(type == "ElevationGrid"){
        mesh = readElevationGrid(info);
    } else if(type == "TriangleMesh"){
        mesh = readTriangleMesh(info);
    } else if(type == "IndexedFaceSet"){
        mesh = readIndexedFaceSet(info);
    } else if(type == "Resource"){
        mesh = readResourceAsGeometry(info);
    } else {
        typeNode.throwException(
            format(_("Unknown geometry \"{}\""), type));
    }
    return mesh;
}


void StdSceneReader::Impl::readDivisionNumbers(Mapping& info, SgMesh* mesh)
{
    int n;
    if(info.read({ "division_number", "divisionNumber" }, n)){
        mesh->setDivisionNumber(n);
    }
    if(info.read("extra_division_number", n)){
        mesh->setExtraDivisionNumber(n);
    }
}
    

SgMesh* StdSceneReader::Impl::readBox(Mapping& info)
{
    SgMeshPtr mesh = new SgMesh;

    readDivisionNumbers(info, mesh);

    SgMesh::Box box;
    read(info, "size", box.size);
    mesh->setPrimitive(box);

    int edv = info.get("extra_division_number", 1);
    string symbol;
    if(info.read("extra_division_mode", symbol)){
        int mode;
        if(symbol == "preferred"){
            mode = SgMesh::ExtraDivisionPreferred;
        } else if(symbol == "x"){
            mode = SgMesh::ExtraDivisionX;
        } else if(symbol == "y"){
            mode = SgMesh::ExtraDivisionY;
        } else if(symbol == "z"){
            mode = SgMesh::ExtraDivisionZ;
        } else {
            mode = SgMesh::ExtraDivisionPreferred;
        }
        mesh->setExtraDivisionMode(mode);
    }

    if(!meshGenerator.updateMeshWithPrimitiveInformation(mesh)){
        info.throwException(_("A box cannot be generated with the given parameters."));
    }

    return mesh.retn();
}


SgMesh* StdSceneReader::Impl::readSphere(Mapping& info)
{
    SgMeshPtr mesh = new SgMesh;

    readDivisionNumbers(info, mesh);

    SgMesh::Sphere sphere;
    info.read("radius", sphere.radius);
    mesh->setPrimitive(sphere);

    if(!meshGenerator.updateMeshWithPrimitiveInformation(mesh)){
        info.throwException(_("A sphere cannot be generated with the given parameters."));
    }
    return mesh.retn();
}


SgMesh* StdSceneReader::Impl::readCylinder(Mapping& info)
{
    SgMeshPtr mesh = new SgMesh;
    
    readDivisionNumbers(info, mesh);

    SgMesh::Cylinder cylinder;
    info.read("radius", cylinder.radius);
    info.read("height", cylinder.height);
    info.read("top", cylinder.top);
    info.read("bottom", cylinder.bottom);
    mesh->setPrimitive(cylinder);

    if(!meshGenerator.updateMeshWithPrimitiveInformation(mesh)){
        info.throwException(_("A cylinder cannot be generated with the given parameters."));
    }
    return mesh.retn();
}


SgMesh* StdSceneReader::Impl::readCone(Mapping& info)
{
    SgMeshPtr mesh = new SgMesh;

    readDivisionNumbers(info, mesh);

    SgMesh::Cone cone;
    info.read("radius", cone.radius);
    info.read("height", cone.height);
    info.read("bottom", cone.bottom);
    mesh->setPrimitive(cone);

    if(!meshGenerator.updateMeshWithPrimitiveInformation(mesh)){
        info.throwException(_("A cone cannot be generated with the given parameters."));
    }
    return mesh.retn();
}


SgMesh* StdSceneReader::Impl::readCapsule(Mapping& info)
{
    SgMeshPtr mesh = new SgMesh;

    readDivisionNumbers(info, mesh);

    SgMesh::Capsule capsule;
    info.read("radius", capsule.radius);
    info.read("height", capsule.height);
    mesh->setPrimitive(capsule);
    
    if(!meshGenerator.updateMeshWithPrimitiveInformation(mesh)){
        info.throwException(_("A capsule cannot be generated with the given parameters."));
    }
    return mesh.retn();
}


SgMesh* StdSceneReader::Impl::readExtrusion(Mapping& info)
{
    MeshGenerator::Extrusion extrusion;

    Listing& crossSectionNode = *info.findListing({ "cross_section", "crossSection" });
    if(crossSectionNode.isValid()){
        const int n = crossSectionNode.size() / 2;
        MeshGenerator::Vector2Array& crossSection = extrusion.crossSection;
        crossSection.resize(n);
        for(int i=0; i < n; ++i){
            Vector2& s = crossSection[i];
            for(int j=0; j < 2; ++j){
                s[j] = crossSectionNode[i*2+j].toDouble();
            }
        }
    }

    Listing& spineNode = *info.findListing("spine");
    if(spineNode.isValid()){
        const int n = spineNode.size() / 3;
        MeshGenerator::Vector3Array& spine = extrusion.spine;
        spine.resize(n);
        for(int i=0; i < n; ++i){
            Vector3& s = spine[i];
            for(int j=0; j < 3; ++j){
                s[j] = spineNode[i*3+j].toDouble();
            }
        }
    }

    Listing& orientationNode = *info.findListing("orientation");
    if(orientationNode.isValid()){
        const int n = orientationNode.size() / 4;
        MeshGenerator::AngleAxisArray& orientation = extrusion.orientation;
        orientation.resize(n);
        for(int i=0; i < n; ++i){
            AngleAxis& aa = orientation[i];
            Vector3& axis = aa.axis();
            for(int j=0; j < 3; ++j){
                axis[j] = orientationNode[i*4+j].toDouble();
            }
            aa.angle() = self->toRadian(orientationNode[i*4+3].toDouble());
        }
    }
    
    Listing& scaleNode = *info.findListing("scale");
    if(scaleNode.isValid()){
        const int n = scaleNode.size() / 2;
        MeshGenerator::Vector2Array& scale = extrusion.scale;
        scale.resize(n);
        for(int i=0; i < n; ++i){
            Vector2& s = scale[i];
            for(int j=0; j < 2; ++j){
                s[j] = scaleNode[i*2+j].toDouble();
            }
        }
    }

    readAngle(info, { "crease_angle", "creaseAngle" }, extrusion.creaseAngle);
    info.read({" begin_cap", "beginCap" }, extrusion.beginCap);
    info.read({ "end_cap", "endCap"}, extrusion.endCap);

    SgMesh* mesh = meshGenerator.generateExtrusion(extrusion, generateTexCoord);

    mesh->setSolid(info.get("solid", mesh->isSolid()));
    
    return mesh;
}


SgMesh* StdSceneReader::Impl::readElevationGrid(Mapping& info)
{
    MeshGenerator::ElevationGrid grid;

    info.read({ "x_dimension", "xDimension" }, grid.xDimension);
    info.read({ "z_dimension", "zDimension" }, grid.zDimension);
    info.read({ "x_spacing", "xSpacing" }, grid.xSpacing);
    info.read({ "z_spacing", "zSpacing" }, grid.zSpacing);
    info.read("ccw", grid.ccw);
    readAngle(info, { "crease_angle", "creaseAngle" }, grid.creaseAngle);

    Listing& heightNode = *info.findListing("height");
    if(heightNode.isValid()){
        for(int i=0; i < heightNode.size(); i++){
            grid.height.push_back(heightNode[i].toDouble());
        }
    }

    SgTexCoordArray* texCoord = nullptr;
    Listing& texCoordNode = *info.findListing({ "tex_coord", "texCoord" });
    if(texCoordNode.isValid()){
        const int size = texCoordNode.size() / 2;
        texCoord = new SgTexCoordArray();
        texCoord->resize(size);
        for(int i=0; i < size; ++i){
            Vector2f& s = texCoord->at(i);
            for(int j=0; j < 2; ++j){
                s[j] = texCoordNode[i*2+j].toDouble();
            }
        }
        generateTexCoord = false;
    }

    SgMesh* mesh = meshGenerator.generateElevationGrid(grid, generateTexCoord);
    if(texCoord){
        mesh->setTexCoords(texCoord);
        mesh->texCoordIndices() = mesh->triangleVertices();
    }

    mesh->setSolid(info.get("solid", mesh->isSolid()));

    return mesh;
}


SgMesh* StdSceneReader::Impl::readTriangleMesh(Mapping& info)
{
    SgMesh* mesh = new SgMesh;

    Listing& srcVertices = *info.findListing("vertices");
    if(srcVertices.isValid()){
        const int size = srcVertices.size() / 3;
        SgVertexArray& vertices = *mesh->setVertices(new SgVertexArray());
        vertices.resize(size);
        for(int i=0; i < size; ++i){
            Vector3f& s = vertices[i];
            for(int j=0; j < 3; ++j){
                s[j] = srcVertices[i*3+j].toDouble();
            }
        }
    }

    Listing& srcTriangles = *info.findListing("triangles");
    if(srcTriangles.isValid()){
        SgIndexArray& triangles = mesh->triangleVertices();
        const int size = srcTriangles.size();
        triangles.reserve(size);
        for(int i=0; i < size; ++i){
            triangles.push_back(srcTriangles[i].toInt());
        }
    }

    double creaseAngle = 0.0;
    self->readAngle(info, "crease_angle", creaseAngle);
    meshFilter.generateNormals(mesh, creaseAngle);

    mesh->setSolid(info.get("solid", mesh->isSolid()));

    return mesh;
}


SgMesh* StdSceneReader::Impl::readIndexedFaceSet(Mapping& info)
{
    SgPolygonMeshPtr polygonMesh = new SgPolygonMesh;

    Listing& coordinateNode = *info.findListing({ "vertices", "coordinate" });
    if(coordinateNode.isValid()){
        const int size = coordinateNode.size() / 3;
        SgVertexArray& vertices = *polygonMesh->setVertices(new SgVertexArray());
        vertices.resize(size);
        for(int i=0; i < size; ++i){
            Vector3f& s = vertices[i];
            for(int j=0; j < 3; ++j){
                s[j] = coordinateNode[i*3+j].toDouble();
            }
        }
    }

    Listing& coordIndexNode = *info.findListing({ "faces", "coordIndex" });
    if(coordIndexNode.isValid()){
        SgIndexArray& polygonVertices = polygonMesh->polygonVertices();
        const int size = coordIndexNode.size();
        polygonVertices.reserve(size);
        for(int i=0; i < size; ++i){
            polygonVertices.push_back(coordIndexNode[i].toInt());
        }
    }

    Listing& texCoordNode = *info.findListing({ "tex_coords", "texCoord" });
    if(texCoordNode.isValid()){
        const int size = texCoordNode.size() / 2;
        SgTexCoordArray& texCoord = *polygonMesh->setTexCoords(new SgTexCoordArray());
        texCoord.resize(size);
        for(int i=0; i < size; ++i){
            Vector2f& s = texCoord[i];
            for(int j=0; j < 2; ++j){
                s[j] = texCoordNode[i*2+j].toDouble();
            }
        }
    }

    Listing& texCoordIndexNode = *info.findListing({ "tex_coord_indices", "texCoordIndex" });
    if(texCoordIndexNode.isValid()){
        SgIndexArray& texCoordIndices = polygonMesh->texCoordIndices();
        const int size = texCoordIndexNode.size();
        texCoordIndices.reserve(size);
        for(int i=0; i<size; i++){
            texCoordIndices.push_back(texCoordIndexNode[i].toInt());
        }
    }

    //polygonMeshTriangulator.setDeepCopyEnabled(true);
    SgMesh* mesh = polygonMeshTriangulator.triangulate(polygonMesh);
    const string& errorMessage = polygonMeshTriangulator.errorMessage();
    if(!errorMessage.empty()){
        info.throwException("Error of an IndexedFaceSet node: \n" + errorMessage);
    }

    if(generateTexCoord){
        if(mesh && !mesh->hasTexCoords()){
            meshGenerator.generateTextureCoordinateForIndexedFaceSet(mesh);
        }
    }

    double creaseAngle = 0.0;
    readAngle(info, { "crease_angle", "creaseAngle" }, creaseAngle);
    meshFilter.generateNormals(mesh, creaseAngle);

    mesh->setSolid(info.get("solid", mesh->isSolid()));

    return mesh;
}


SgMesh* StdSceneReader::Impl::readResourceAsGeometry(Mapping& info)
{
    SgMesh* mesh = nullptr;
    SgNode* resource = readResource(info);
    if(resource){
        SgShape* shape = dynamic_cast<SgShape*>(resource);
        if(!shape){
            info.throwException(_("A resouce specified as a geometry must be a single mesh"));
        }
        mesh = shape->mesh();
        double creaseAngle;
        if(readAngle(info, { "crease_angle", "creaseAngle" }, creaseAngle)){
            mesh->setCreaseAngle(creaseAngle);
            meshFilter.setNormalOverwritingEnabled(true);
            bool removeRedundantVertices =
                info.get({ "remove_redundant_vertices", "removeRedundantVertices" }, false);
            meshFilter.generateNormals(mesh, creaseAngle, removeRedundantVertices);
            meshFilter.setNormalOverwritingEnabled(false);
        }
        if(generateTexCoord){
            if(mesh && !mesh->hasTexCoords()){
                meshGenerator.generateTextureCoordinateForIndexedFaceSet(mesh);
            }
        }
    }
    return mesh;
}


void StdSceneReader::Impl::readAppearance(SgShape* shape, Mapping& info)
{
    Mapping& material = *info.findMapping("material");
    if(material.isValid()){
        readMaterial(shape, material);
    }

    Mapping& texture = *info.findMapping("texture");
    if(texture.isValid()){
        readTexture(shape, texture);

        Mapping& textureTransform = *info.findMapping({ "texture_transform", "textureTransform" });
        if(textureTransform.isValid() && shape->texture()){
            readTextureTransform(shape->texture(), textureTransform);
        }
    }
}


void StdSceneReader::Impl::readMaterial(SgShape* shape, Mapping& info)
{
    SgMaterialPtr material = new SgMaterial;

    double value;
    if(info.read({ "ambient", "ambientIntensity" }, value)){
        material->setAmbientIntensity(value);
    }
    if(read(info, { "diffuse", "diffuseColor" }, color)){
        material->setDiffuseColor(color);
    }
    if(read(info, { "emissive", "emissiveColor" }, color)){
        material->setEmissiveColor(color);
    }
    if(read(info, { "specular", "specularColor" }, color)){
        material->setSpecularColor(color);
    }
    if(info.read("specular_exponent", value)){
        material->setSpecularExponent(value);
    } else if(info.read("shininess", value)){ // deprecated
        material->setSpecularExponent(value * 128.0f);
    }
    
    if(info.read("transparency", value)){
        material->setTransparency(value);
    }

    shape->setMaterial(material);
}


void StdSceneReader::Impl::readTexture(SgShape* shape, Mapping& info)
{
    string& uri = symbol;
    if(info.read({ "uri", "url" }, uri) && !uri.empty()){
        SgImagePtr image;
        ImagePathToSgImageMap::iterator p = imagePathToSgImageMap.find(uri);
        if(p != imagePathToSgImageMap.end()){
            image = p->second;
        }else{
            auto fpvp = getOrCreatePathVariableProcessor();
            auto filename = fpvp->expand(uri, true);
            if(filename.empty()){
                os() << format(_("Warning: texture uri \"{0}\" is not valid: {1}"),
                               uri, fpvp->errorMessage()) << endl;
            } else {
                image = new SgImage;
                if(imageIO.load(image->image(), filename, os())){
                    imagePathToSgImageMap[uri] = image;
                } else {
                    image.reset();
                }
            }
        }
        if(image){
            SgTexturePtr texture = new SgTexture;
            texture->setImage(image);
            bool repeatS = true;
            bool repeatT = true;
            
            auto repeatList = info.findListing("repeat");
            if(repeatList->isValid() && repeatList->size() == 2){
                repeatS = repeatList->at(0)->toBool();
                repeatT = repeatList->at(1)->toBool();
            } else {
                info.read("repeatS", repeatS);
                info.read("repeatT", repeatT);
            }
            texture->setRepeat(repeatS, repeatT);
            texture->setTextureTransform(new SgTextureTransform);
            shape->setTexture(texture);
        }
    }
}


void StdSceneReader::Impl::readTextureTransform(SgTexture* texture, Mapping& info)
{
    SgTextureTransform* textureTransform = texture->textureTransform();
    if(read(info, "center", v2)) textureTransform->setCenter(v2);
    if(read(info, "scale", v2)) textureTransform->setScale(v2);
    if(read(info, "translation", v2)) textureTransform->setTranslation(v2);
    if(self->readAngle(info, "rotation", value)) textureTransform->setRotation(value);
}


void StdSceneReader::Impl::readLightCommon(Mapping& info, SgLight* light)
{
    if(info.read("on", on)) light->on(on);
    if(read(info, "color", color)) light->setColor(color);
    if(info.read("intensity", value)) light->setIntensity(value);
    if(info.read("ambientIntensity", value)) light->setAmbientIntensity(value);
}


SgNode* StdSceneReader::Impl::readDirectionalLight(Mapping& info)
{
    SgDirectionalLightPtr light = new SgDirectionalLight;
    readLightCommon(info, light);
    if(read(info, "direction", v)) light->setDirection(v);
    return light.retn();
}


SgNode* StdSceneReader::Impl::readSpotLight(Mapping& info)
{
    SgSpotLightPtr light = new SgSpotLight;

    readLightCommon(info, light);

    if(read(info, "direction", v)) light->setDirection(v);
    if(readAngle(info, { "beam_width", "beamWidth" }, value)) light->setBeamWidth(value);
    if(readAngle(info, { "cut_off_angle", "cutOffAngle" }, value)) light->setCutOffAngle(value);
    if(info.read({ "cut_off_exponent", "cutOffExponent" }, value)) light->setCutOffExponent(value);
    if(read(info, "attenuation", color)){
        light->setConstantAttenuation(color[0]);
        light->setLinearAttenuation(color[1]);
        light->setQuadraticAttenuation(color[2]);
    }
    
    return light.retn();
}


SgNode* StdSceneReader::Impl::readResource(Mapping& info)
{
    auto resource = readResourceNode(info);
    if(resource.scene){
        return resource.scene;
    } else if(resource.info){
        return readNode(*resource.info->toMapping());
    }
    return nullptr;
}


StdSceneReader::Resource StdSceneReader::readResourceNode(Mapping& info)
{
    return impl->readResourceNode(info);
}


StdSceneReader::Resource StdSceneReader::Impl::readResourceNode(Mapping& info)
{
    string uri = info["uri"].toString();

    ValueNode& exclude = *info.find("exclude");
    if(exclude.isValid()){
        if(exclude.isString()){
            decoupleResourceNode(info, uri, exclude.toString());
        } else if(exclude.isListing()){
            Listing& excludes = *exclude.toListing();
            for(auto& nodeToExclude : excludes){
                decoupleResourceNode(info, uri, nodeToExclude->toString());
            }
        } else {
            exclude.throwException(_("The value of \"exclude\" must be string or sequence."));
        }
    }
        
    auto resource = loadResource(info, uri);

    if(resource.scene){
        resource.scene = readTransformParameters(info, resource.scene);
    }

    return resource;
}


StdSceneReader::Resource StdSceneReader::Impl::loadResource(Mapping& resourceNode, const string& uri)
{
    vector<string> names;
    auto node = resourceNode.find("node");
    if(node->isValid()){
        if(node->isString()){
            names.push_back(node->toString());
        } else if(node->isListing()){
            auto nodes = node->toListing();
            for(auto& nodei : *nodes){
                names.push_back(nodei->toString());
            }
        }
    }
    
    Resource resource;
    ResourceInfo* resourceInfo = getOrCreateResourceInfo(resourceNode, uri);
    if(resourceInfo){
        resource.directory = resourceInfo->directory;
        bool isYamlResouce = (resourceInfo->yamlReader != nullptr);
        if(names.empty()){
            if(isYamlResouce){
                resource.info = resourceInfo->yamlReader->document();
            } else {
                resource.scene = resourceInfo->scene;
            }
        } else {
            if(isYamlResouce){
                extractNamedYamlNodes(resourceNode, resourceInfo, names, uri, resource);
            } else {
                extractNamedSceneNodes(resourceNode, resourceInfo, names, uri, resource);
            }
        }
    }
    
    return resource;
}


void StdSceneReader::Impl::extractNamedYamlNodes
(Mapping& resourceNode, ResourceInfo* info, vector<string>& names, const string& uri, Resource& resource)
{
    Listing* group = nullptr;
    if(names.size() >= 2){
        group = new Listing;
        resource.info = group;
    }
    for(auto& name : names){
        auto node = info->yamlReader->findAnchoredNode(name);
        if(!node){
            resourceNode.throwException(
                format(_("Node \"{0}\" is not found in \"{1}\"."), name, uri));
        }
        if(group){
            group->append(node);
        } else {
            resource.info = node;
        }
    }
}


void StdSceneReader::Impl::extractNamedSceneNodes
(Mapping& resourceNode, ResourceInfo* info, vector<string>& names, const string& uri, Resource& resource)
{
    unique_ptr<SceneNodeMap>& nodeMap = info->sceneNodeMap;
    if(!nodeMap){
        makeSceneNodeMap(info);
    }

    SgGroupPtr group;
    if(names.size() >= 2){
        group = new SgGroup;
        resource.scene = group;
    }

    for(auto& name : names){
        auto iter = nodeMap->find(name);
        if(iter == nodeMap->end()){
            resourceNode.throwException(
                format(_("Node \"{0}\" is not found in \"{1}\"."), name, uri));
        } else {
            SceneNodeInfo& nodeInfo = iter->second;
            if(nodeInfo.parent){
                nodeInfo.parent->removeChild(nodeInfo.node);
                nodeInfo.parent.reset();
                adjustNodeCoordinate(nodeInfo);
            }
            if(group){
                group->addChild(nodeInfo.node);
            } else {
                resource.scene = nodeInfo.node;
            }
        }
    }
}


void StdSceneReader::Impl::decoupleResourceNode(Mapping& resourceNode, const string& uri, const string& nodeName)
{
    ResourceInfo* resourceInfo = getOrCreateResourceInfo(resourceNode, uri);
    if(resourceInfo){
        unique_ptr<SceneNodeMap>& nodeMap = resourceInfo->sceneNodeMap;
        if(!nodeMap){
            makeSceneNodeMap(resourceInfo);
        }
        auto iter = nodeMap->find(nodeName);
        if(iter != nodeMap->end()){
            SceneNodeInfo& nodeInfo = iter->second;
            if(nodeInfo.parent){
                nodeInfo.parent->removeChild(nodeInfo.node);
                nodeInfo.parent.reset();
                adjustNodeCoordinate(nodeInfo);
            }
        }
    }
}


StdSceneReader::Impl::ResourceInfo*
StdSceneReader::Impl::getOrCreateResourceInfo(Mapping& resourceNode, const string& uri)
{
    auto iter = resourceInfoMap.find(uri);

    if(iter != resourceInfoMap.end()){
        return iter->second;
    }

    if(!isUriSchemeRegexReady){
        uriSchemeRegex.assign("^(.+)://(.+)$");
        isUriSchemeRegexReady = true;
    }
    smatch match;
    string filename;
    bool hasScheme = false;
    bool isFileScheme = false;
        
    if(regex_match(uri, match, uriSchemeRegex)){
        hasScheme = true;
        if(match.size() == 3){
            const string scheme = match.str(1);
            if(scheme == "file"){
                filename = match.str(2);
                isFileScheme = true;
            } else {
                std::lock_guard<std::mutex> guard(uriSchemeHandlerMutex);
                auto iter = uriSchemeHandlerMap.find(scheme);
                if(iter == uriSchemeHandlerMap.end()){
                    resourceNode.throwException(
                        format(_("The \"{0}\" scheme of \"{1}\" is not available"), scheme, uri));
                } else {
                    auto& handler = iter->second;
                    filename = handler(match.str(2), os());
                    isFileScheme = true;
                }
            }
        }
    }

    if(!hasScheme){
        filename = uri;
        isFileScheme = true;
    }

    if(isFileScheme){
        auto pvp = getOrCreatePathVariableProcessor();
        filename = pvp->expand(filename, true);
        if(filename.empty()){
            resourceNode.throwException(
                format(_("The resource URI \"{0}\" is not valid: {1}"),
                       uri, pvp->errorMessage()));
        }
    } else {
        if(filename.empty()){
            resourceNode.throwException(
                format(_("The resource URI \"{}\" is not valid"), uri));
        }
    }

    ResourceInfoPtr info = new ResourceInfo;

    filesystem::path filepath(fromUTF8(filename));
    string ext = filepath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if(ext == ".yaml" || ext == ".yml"){
        unique_ptr<YAMLReader> reader(new YAMLReader);
        reader->importAnchors(*mainYamlReader);
        if(!reader->load(filename)){
            resourceNode.throwException(
                format(_("YAML resource \"{0}\" cannot be loaded ({1})"),
                 uri, reader->errorMessage()));
        }
        info->yamlReader = std::move(reader);

    } else {
        SgNodePtr scene = sceneLoader.load(filename);
        if(!scene){
            resourceNode.throwException(
                format(_("The resource is not found at URI \"{}\""), uri));
        }
        info->scene = scene;
    }

    info->directory = toUTF8(filepath.parent_path().string());
    
    resourceInfoMap[uri] = info;

    return info;
}


void StdSceneReader::Impl::adjustNodeCoordinate(SceneNodeInfo& info)
{
    if(auto pos = dynamic_cast<SgPosTransform*>(info.node.get())){
        if(info.isScaled){
            auto affine = new SgAffineTransform;
            affine->setLinear(info.R * pos->rotation());
            affine->translation().setZero();
            pos->moveChildrenTo(affine);
            info.node = affine;
        } else {
            pos->setRotation(info.R * pos->rotation());
            pos->translation().setZero();
        }
            
    } else if(auto affine = dynamic_cast<SgAffineTransform*>(info.node.get())){
        affine->setLinear(info.R * affine->linear());
        affine->translation().setZero();
            
    } else {
        if(info.isScaled){
            auto transform = new SgAffineTransform;
            transform->setLinear(info.R);
            transform->translation().setZero();
            transform->addChild(info.node);
            info.node = transform;
        } else if(!info.R.isApprox(Matrix3::Identity())){
            auto transform = new SgPosTransform;
            transform->setRotation(info.R);
            transform->translation().setZero();
            transform->addChild(info.node);
            info.node = transform;
        }
    }
}
        

void StdSceneReader::Impl::makeSceneNodeMap(ResourceInfo* info)
{
    info->sceneNodeMap.reset(new SceneNodeMap);
    SceneNodeInfo nodeInfo;
    nodeInfo.parent = 0;
    nodeInfo.node = info->scene;
    nodeInfo.R = Matrix3::Identity();
    nodeInfo.isScaled = false;
    makeSceneNodeMapSub(nodeInfo, *info->sceneNodeMap);
}


void StdSceneReader::Impl::makeSceneNodeMapSub(const SceneNodeInfo& nodeInfo, SceneNodeMap& nodeMap)
{
    const string& name = nodeInfo.node->name();
    bool wasProcessed = false;
    if(!name.empty()){
        wasProcessed = !nodeMap.insert(make_pair(name, nodeInfo)).second;
    }
    if(!wasProcessed){
        SgGroup* group = dynamic_cast<SgGroup*>(nodeInfo.node.get());
        if(group){
            SceneNodeInfo childInfo;
            childInfo.parent = group;
            childInfo.isScaled = nodeInfo.isScaled;
            SgTransform* transform = dynamic_cast<SgTransform*>(group);
            if(!transform){
                childInfo.R = nodeInfo.R;
            } else if(auto pos = dynamic_cast<SgPosTransform*>(transform)){
                childInfo.R = nodeInfo.R * pos->rotation();
            } else {
                Affine3 T;
                transform->getTransform(T);
                childInfo.R = nodeInfo.R * T.linear();
                childInfo.isScaled = true;
            }
            for(SgNode* child : *group){
                childInfo.node = child;
                makeSceneNodeMapSub(childInfo, nodeMap);
            }
        }
    }
}
