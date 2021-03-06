#ifndef COMPONENTS_TERRAIN_TERRAINGRID_H
#define COMPONENTS_TERRAIN_TERRAINGRID_H

#include <osg/Vec2f>

#include "world.hpp"
#include "material.hpp"

namespace SceneUtil
{
    class UnrefQueue;
}

namespace Terrain
{

    /// @brief Simple terrain implementation that loads cells in a grid, with no LOD
    class TerrainGrid : public Terrain::World
    {
    public:
        TerrainGrid(osg::Group* parent, Resource::ResourceSystem* resourceSystem, osgUtil::IncrementalCompileOperation* ico, Storage* storage, int nodeMask, SceneUtil::UnrefQueue* unrefQueue = NULL);
        ~TerrainGrid();

        /// Load a terrain cell and store it in cache for later use.
        /// @note The returned ref_ptr should be kept by the caller to ensure that the terrain stays in cache for as long as needed.
        /// @note Thread safe.
        virtual osg::ref_ptr<osg::Node> cacheCell(int x, int y);

        /// @note Not thread safe.
        virtual void loadCell(int x, int y);

        /// @note Not thread safe.
        virtual void unloadCell(int x, int y);

        /// Clear cached objects that are no longer referenced
        /// @note Thread safe.
        void updateCache();

    private:
        osg::ref_ptr<osg::Node> buildTerrain (osg::Group* parent, float chunkSize, const osg::Vec2f& chunkCenter);

        // split each ESM::Cell into mNumSplits*mNumSplits terrain chunks
        unsigned int mNumSplits;

        typedef std::map<std::string, osg::ref_ptr<osg::Texture2D> >  TextureCache;
        TextureCache mTextureCache;
        OpenThreads::Mutex mTextureCacheMutex;

        typedef std::map<std::pair<int, int>, osg::ref_ptr<osg::Node> > Grid;
        Grid mGrid;

        Grid mGridCache;
        OpenThreads::Mutex mGridCacheMutex;

        BufferCache mCache;

        osg::ref_ptr<SceneUtil::UnrefQueue> mUnrefQueue;
    };

}

#endif
