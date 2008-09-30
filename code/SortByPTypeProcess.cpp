/*
---------------------------------------------------------------------------
Open Asset Import Library (ASSIMP)
---------------------------------------------------------------------------

Copyright (c) 2006-2008, ASSIMP Development Team

All rights reserved.

Redistribution and use of this software in source and binary forms, 
with or without modification, are permitted provided that the following 
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the ASSIMP team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the ASSIMP Development Team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

/** @file Implementation of the DeterminePTypeHelperProcess and
 *  SortByPTypeProcess post-process steps.
*/

// public ASSIMP headers
#include "../include/DefaultLogger.h"
#include "../include/aiPostProcess.h"
#include "../include/aiScene.h"

// internal headers
#include "ProcessHelper.h"
#include "SortByPTypeProcess.h"
#include "qnan.h"

using namespace Assimp;

// ------------------------------------------------------------------------------------------------
// Constructor to be privately used by Importer
DeterminePTypeHelperProcess ::DeterminePTypeHelperProcess()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Destructor, private as well
DeterminePTypeHelperProcess::~DeterminePTypeHelperProcess()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Returns whether the processing step is present in the given flag field.
bool DeterminePTypeHelperProcess::IsActive( unsigned int pFlags) const
{
	// this step is always active
	return	true;
}

// ------------------------------------------------------------------------------------------------
// Executes the post processing step on the given imported data.
void DeterminePTypeHelperProcess::Execute( aiScene* pScene)
{
	for (unsigned int i = 0; i < pScene->mNumMeshes;++i)
	{
		aiMesh* mesh = pScene->mMeshes[i];
		if (!mesh->mPrimitiveTypes)
		{
			bool bDeg = false;
			for (unsigned int a = 0; a < mesh->mNumFaces; ++a)
			{
				aiFace& face = mesh->mFaces[a];
				switch (face.mNumIndices)
				{
				case 3u:

					// check whether the triangle is degenerated
					if (mesh->mVertices[face.mIndices[0]] == mesh->mVertices[face.mIndices[1]] ||
					    mesh->mVertices[face.mIndices[1]] == mesh->mVertices[face.mIndices[2]])
					{
						face.mNumIndices = 2;
						unsigned int* pi = new unsigned int[2];
						pi[0] = face.mIndices[0];
						pi[1] = face.mIndices[2];
						delete[] face.mIndices;
						face.mIndices = pi;

						bDeg = true;
					}
					else if (mesh->mVertices[face.mIndices[2]] == mesh->mVertices[face.mIndices[0]])
					{
						face.mNumIndices = 2;
						unsigned int* pi = new unsigned int[2];
						pi[0] = face.mIndices[0];
						pi[1] = face.mIndices[1];
						delete[] face.mIndices;
						face.mIndices = pi;

						bDeg = true;
					}
					else
					{
						mesh->mPrimitiveTypes |= aiPrimitiveType_TRIANGLE;
						break;
					}
				case 2u:

					// check whether the line is degenerated
					if (mesh->mVertices[face.mIndices[0]] == mesh->mVertices[face.mIndices[1]])
					{
						face.mNumIndices = 1;
						unsigned int* pi = new unsigned int[1];
						pi[0] = face.mIndices[0];
						delete[] face.mIndices;
						face.mIndices = pi;

						bDeg = true;
					}
					else
					{
						mesh->mPrimitiveTypes |= aiPrimitiveType_LINE;
						break;
					}
				case 1u:
					mesh->mPrimitiveTypes |= aiPrimitiveType_POINT;
					break;
				default:
					mesh->mPrimitiveTypes |= aiPrimitiveType_POLYGON;
					break;
				}
			}
			if (bDeg)
			{
				DefaultLogger::get()->warn("Found degenerated primitives");
			}
		}
	}
}



// ------------------------------------------------------------------------------------------------
// Constructor to be privately used by Importer
SortByPTypeProcess::SortByPTypeProcess()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Destructor, private as well
SortByPTypeProcess::~SortByPTypeProcess()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Returns whether the processing step is present in the given flag field.
bool SortByPTypeProcess::IsActive( unsigned int pFlags) const
{
	return	(pFlags & aiProcess_SortByPType) != 0;
}

// ------------------------------------------------------------------------------------------------
// Update changed meshes in all nodes
void UpdateNodes(const std::vector<unsigned int>& replaceMeshIndex, aiNode* node)
{
	std::vector<unsigned int>::const_iterator it;

	if (node->mNumMeshes)
	{
		unsigned int newSize = 0;
		for (unsigned int m = 0; m< node->mNumMeshes; ++m)
		{
			it = replaceMeshIndex.begin()+(node->mMeshes[m]*5u);
			for (;*it != 0xcdcdcdcd;++it)
			{
				if (0xffffffff != *it)++newSize;
			}
		}

		ai_assert(0 != newSize);

		unsigned int* newMeshes = new unsigned int[newSize];
		for (unsigned int m = 0; m< node->mNumMeshes; ++m)
		{
			it = replaceMeshIndex.begin()+(node->mMeshes[m]*5u);
			for (;*it != 0xcdcdcdcd;++it)
			{
				if (0xffffffff != *it)*newMeshes++ = *it;
			}
		}
		delete[] node->mMeshes;
		node->mMeshes = newMeshes-(node->mNumMeshes = newSize);
	}

	// call all subnodes recursively
	for (unsigned int m = 0; m < node->mNumChildren; ++m)
		UpdateNodes(replaceMeshIndex,node->mChildren[m]);
}

// ------------------------------------------------------------------------------------------------
// Executes the post processing step on the given imported data.
void SortByPTypeProcess::Execute( aiScene* pScene)
{
	if (!pScene->mNumMeshes)return;

	std::vector<aiMesh*> outMeshes;
	outMeshes.reserve(pScene->mNumMeshes<<1u);

	std::vector<unsigned int> replaceMeshIndex(pScene->mNumMeshes*5,0xffffffff);
	std::vector<unsigned int>::iterator meshIdx = replaceMeshIndex.begin();
	for (unsigned int i = 0; i < pScene->mNumMeshes;++i)
	{
		aiMesh* mesh = pScene->mMeshes[i];
		ai_assert(0 != mesh->mPrimitiveTypes);

		// if there's just one primitive type in the mesh there's nothing to do for us
		unsigned int num = 0;
		if (mesh->mPrimitiveTypes & aiPrimitiveType_POINT)    ++num;
		if (mesh->mPrimitiveTypes & aiPrimitiveType_LINE)     ++num;
		if (mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) ++num;
		if (mesh->mPrimitiveTypes & aiPrimitiveType_POLYGON)  ++num;

		if (1 == num)
		{
			*meshIdx = (unsigned int) outMeshes.size();
			outMeshes.push_back(mesh);
			
			meshIdx += 4;
			*meshIdx = 0xcdcdcdcd;
			++meshIdx;
			continue;
		}
		
		const unsigned int first = (unsigned int)outMeshes.size();

		// reuse our current mesh arrays for the submesh 
		// with the largest numer of primitives
		unsigned int aiNumPerPType[4] = {0,0,0,0};
		aiFace* pFirstFace = mesh->mFaces;
		aiFace* const pLastFace = pFirstFace + mesh->mNumFaces;

		unsigned int numPolyVerts = 0;
		for (;pFirstFace != pLastFace; ++pFirstFace)
		{
			if (pFirstFace->mNumIndices <= 3)
				++aiNumPerPType[pFirstFace->mNumIndices-1];
			else
			{
				++aiNumPerPType[3];
				numPolyVerts += pFirstFace-> mNumIndices;
			}
		}

		VertexWeightTable* avw = ComputeVertexBoneWeightTable(mesh);
		for (unsigned int real = 0; real < 4; ++real,++meshIdx)
		{
			if ( !aiNumPerPType[real])
			{
				continue;
			}

			*meshIdx = (unsigned int) outMeshes.size();
			outMeshes.push_back(new aiMesh());
			aiMesh* out = outMeshes.back();

			// copy data members
			out->mPrimitiveTypes = 1u << real;
			out->mMaterialIndex = mesh->mMaterialIndex;

			// allocate output storage
			out->mNumFaces = aiNumPerPType[real];
			aiFace* outFaces = out->mFaces = new aiFace[out->mNumFaces];

			out->mNumVertices = (3 == real ? numPolyVerts : out->mNumFaces * (real+1));

			aiVector3D *vert(NULL), *nor(NULL), *tan(NULL), *bit(NULL);
			aiVector3D *uv   [AI_MAX_NUMBER_OF_TEXTURECOORDS];
			aiColor4D  *cols [AI_MAX_NUMBER_OF_COLOR_SETS];
		
			if (mesh->mVertices)
				vert = out->mVertices = new aiVector3D[out->mNumVertices];

			if (mesh->mNormals)
				nor  = out->mNormals  = new aiVector3D[out->mNumVertices];

			if (mesh->mTangents)
			{
				tan = out->mTangents   = new aiVector3D[out->mNumVertices];
				bit = out->mBitangents = new aiVector3D[out->mNumVertices];
			}

			for (unsigned int i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS;++i)
			{
				if (mesh->mTextureCoords[i])
					uv[i] = out->mTextureCoords[i] = new aiVector3D[out->mNumVertices];
				else uv[i] = NULL;

				out->mNumUVComponents[i] = mesh->mNumUVComponents[i];
			}

			for (unsigned int i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS;++i)
			{
				if (mesh->mColors[i])
					cols[i] = out->mColors[i] = new aiColor4D[out->mNumVertices];
				else cols[i] = NULL;
			}

			typedef std::vector< aiVertexWeight > TempBoneInfo;
			std::vector< TempBoneInfo > tempBones(mesh->mNumBones);

			// try to guess how much storage we'll need
			for (unsigned int q = 0; q < mesh->mNumBones;++q)
			{
				tempBones[q].reserve(mesh->mBones[q]->mNumWeights / (num-1));
			}

			unsigned int outIdx = 0;
			for (unsigned int m = 0; m < mesh->mNumFaces; ++m)
			{
				aiFace& in = mesh->mFaces[m];
				if ((real == 3  && in.mNumIndices <= 3) || (real != 3 && in.mNumIndices != real+1))
				{
					continue;
				}
				
				outFaces->mNumIndices = in.mNumIndices;
				outFaces->mIndices    = in.mIndices;

				for (unsigned int q = 0; q < in.mNumIndices; ++q)
				{
					register unsigned int idx = in.mIndices[q];

					// process all bones of this index
					if (avw)
					{
						VertexWeightTable& tbl = avw[idx];
						for (VertexWeightTable::const_iterator it = tbl.begin(), end = tbl.end();
							 it != end; ++it)
						{
							tempBones[ (*it).first ].push_back( aiVertexWeight(idx, (*it).second) );
						}
					}

					if (vert)
					{
						*vert++ = mesh->mVertices[idx];
						mesh->mVertices[idx].x = std::numeric_limits<float>::quiet_NaN();
					}
					if (nor )*nor++  = mesh->mNormals[idx];
					if (tan )
					{
						*tan++  = mesh->mTangents[idx];
						*bit++  = mesh->mBitangents[idx];
					}

					for (unsigned int pp = 0; pp < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++pp)
					{
						if (!uv[pp])break;
						*uv[pp]++ = mesh->mTextureCoords[pp][idx];
					}

					for (unsigned int pp = 0; pp < AI_MAX_NUMBER_OF_COLOR_SETS; ++pp)
					{
						if (!cols[pp])break;
						*cols[pp]++ = mesh->mColors[pp][idx];
					}

					in.mIndices[q] = outIdx++;
				}

				in.mIndices = NULL;
				++outFaces;
			}

			// now generate output bones
			for (unsigned int q = 0; q < mesh->mNumBones;++q)
				if (!tempBones[q].empty())++out->mNumBones;

			if (out->mNumBones)
			{
				out->mBones = new aiBone*[out->mNumBones];
				for (unsigned int q = 0, real = 0; q < mesh->mNumBones;++q)
				{
					TempBoneInfo& in = tempBones[q];
					if (in.empty())continue;

					aiBone* srcBone = mesh->mBones[q];
					aiBone* bone = out->mBones[real] = new aiBone();

					bone->mName = srcBone->mName;
					bone->mOffsetMatrix = srcBone->mOffsetMatrix;

					bone->mNumWeights = (unsigned int)in.size();
					bone->mWeights = new aiVertexWeight[bone->mNumWeights];

					::memcpy(bone->mWeights,&in[0],bone->mNumWeights*sizeof(void*));

					++real;
				}
			}
		}

		*meshIdx = 0xcdcdcdcd;
		++meshIdx;

		// delete the per-vertex bone weights table
		delete[] avw;

		// delete the input mesh
		delete mesh;
	}

	UpdateNodes(replaceMeshIndex,pScene->mRootNode);

	if (outMeshes.size() != pScene->mNumMeshes)
	{
		delete[] pScene->mMeshes;
		pScene->mNumMeshes = (unsigned int)outMeshes.size();
		pScene->mMeshes = new aiMesh*[pScene->mNumMeshes];
		::memcpy(pScene->mMeshes,&outMeshes[0],pScene->mNumMeshes*sizeof(void*));
	}
}

