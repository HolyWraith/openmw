#include "bulletshapemanager.hpp"

#include <osg/NodeVisitor>
#include <osg/Geode>
#include <osg/TriangleFunctor>

#include <BulletCollision/CollisionShapes/btTriangleMesh.h>

#include <components/vfs/manager.hpp>

#include <components/nifbullet/bulletnifloader.hpp>

#include "bulletshape.hpp"
#include "scenemanager.hpp"
#include "niffilemanager.hpp"
#include "objectcache.hpp"
#include "multiobjectcache.hpp"

namespace Resource
{

struct GetTriangleFunctor
{
    GetTriangleFunctor()
        : mTriMesh(NULL)
    {
    }

    void setTriMesh(btTriangleMesh* triMesh)
    {
        mTriMesh = triMesh;
    }

    void setMatrix(const osg::Matrixf& matrix)
    {
        mMatrix = matrix;
    }

    inline btVector3 toBullet(const osg::Vec3f& vec)
    {
        return btVector3(vec.x(), vec.y(), vec.z());
    }

    void inline operator()( const osg::Vec3 v1, const osg::Vec3 v2, const osg::Vec3 v3, bool _temp )
    {
        if (mTriMesh)
            mTriMesh->addTriangle( toBullet(mMatrix.preMult(v1)), toBullet(mMatrix.preMult(v2)), toBullet(mMatrix.preMult(v3)));
    }

    btTriangleMesh* mTriMesh;
    osg::Matrixf mMatrix;
};

/// Creates a BulletShape out of a Node hierarchy.
class NodeToShapeVisitor : public osg::NodeVisitor
{
public:
    NodeToShapeVisitor()
        : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
        , mTriangleMesh(NULL)
    {

    }

    virtual void apply(osg::Geode& geode)
    {
        for (unsigned int i=0; i<geode.getNumDrawables(); ++i)
            apply(*geode.getDrawable(i));
    }

    virtual void apply(osg::Drawable &drawable)
    {
        if (!mTriangleMesh)
            mTriangleMesh = new btTriangleMesh;

        osg::Matrixf worldMat = osg::computeLocalToWorld(getNodePath());
        osg::TriangleFunctor<GetTriangleFunctor> functor;
        functor.setTriMesh(mTriangleMesh);
        functor.setMatrix(worldMat);
        drawable.accept(functor);
    }

    osg::ref_ptr<BulletShape> getShape()
    {
        if (!mTriangleMesh)
            return osg::ref_ptr<BulletShape>();

        osg::ref_ptr<BulletShape> shape (new BulletShape);
        TriangleMeshShape* meshShape = new TriangleMeshShape(mTriangleMesh, true);
        shape->mCollisionShape = meshShape;
        mTriangleMesh = NULL;
        return shape;
    }

private:
    btTriangleMesh* mTriangleMesh;
};

BulletShapeManager::BulletShapeManager(const VFS::Manager* vfs, SceneManager* sceneMgr, NifFileManager* nifFileManager)
    : ResourceManager(vfs)
    , mInstanceCache(new MultiObjectCache)
    , mSceneManager(sceneMgr)
    , mNifFileManager(nifFileManager)
{

}

BulletShapeManager::~BulletShapeManager()
{

}

osg::ref_ptr<const BulletShape> BulletShapeManager::getShape(const std::string &name)
{
    std::string normalized = name;
    mVFS->normalizeFilename(normalized);

    osg::ref_ptr<BulletShape> shape;
    osg::ref_ptr<osg::Object> obj = mCache->getRefFromObjectCache(normalized);
    if (obj)
        shape = osg::ref_ptr<BulletShape>(static_cast<BulletShape*>(obj.get()));
    else
    {
        size_t extPos = normalized.find_last_of('.');
        std::string ext;
        if (extPos != std::string::npos && extPos+1 < normalized.size())
            ext = normalized.substr(extPos+1);

        if (ext == "nif")
        {
            NifBullet::BulletNifLoader loader;
            shape = loader.load(mNifFileManager->get(normalized));
        }
        else
        {
            // TODO: support .bullet shape files

            osg::ref_ptr<const osg::Node> constNode (mSceneManager->getTemplate(normalized));
            osg::ref_ptr<osg::Node> node (const_cast<osg::Node*>(constNode.get())); // const-trickery required because there is no const version of NodeVisitor
            NodeToShapeVisitor visitor;
            node->accept(visitor);
            shape = visitor.getShape();
            if (!shape)
            {
                mCache->addEntryToObjectCache(normalized, NULL);
                return osg::ref_ptr<BulletShape>();
            }
        }

        mCache->addEntryToObjectCache(normalized, shape);
    }
    return shape;
}

osg::ref_ptr<BulletShapeInstance> BulletShapeManager::cacheInstance(const std::string &name)
{
    std::string normalized = name;
    mVFS->normalizeFilename(normalized);

    osg::ref_ptr<BulletShapeInstance> instance = createInstance(normalized);
    mInstanceCache->addEntryToObjectCache(normalized, instance.get());
    return instance;
}

osg::ref_ptr<BulletShapeInstance> BulletShapeManager::getInstance(const std::string &name)
{
    std::string normalized = name;
    mVFS->normalizeFilename(normalized);

    osg::ref_ptr<osg::Object> obj = mInstanceCache->takeFromObjectCache(normalized);
    if (obj.get())
        return static_cast<BulletShapeInstance*>(obj.get());
    else
        return createInstance(normalized);
}

osg::ref_ptr<BulletShapeInstance> BulletShapeManager::createInstance(const std::string &name)
{
    osg::ref_ptr<const BulletShape> shape = getShape(name);
    if (shape)
        return shape->makeInstance();
    else
        return osg::ref_ptr<BulletShapeInstance>();
}

void BulletShapeManager::updateCache(double referenceTime)
{
    ResourceManager::updateCache(referenceTime);

    mInstanceCache->removeUnreferencedObjectsInCache();
}

}
