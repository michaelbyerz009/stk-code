//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2010-2015  Joerg Henrichs
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "graphics/slip_stream.hpp"
#include "graphics/central_settings.hpp"
#include "config/user_config.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/material_manager.hpp"
#include "graphics/sp/sp_dynamic_draw_call.hpp"
#include "graphics/sp/sp_mesh.hpp"
#include "graphics/sp/sp_mesh_node.hpp"
#include "graphics/sp/sp_shader_manager.hpp"
#include "graphics/sp/sp_uniform_assigner.hpp"
#include "io/file_manager.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/controller/controller.hpp"
#include "karts/kart_properties.hpp"
#include "karts/max_speed.hpp"
#include "modes/world.hpp"
#include "tracks/quad.hpp"
#include "utils/constants.hpp"
#include "utils/mini_glm.hpp"

/** Creates the slip stream object using a moving texture.
 *  \param kart Pointer to the kart to which the slip stream
 *              belongs to.
 */
SlipStream::SlipStream(AbstractKart* kart) : MovingTexture(0, 0), m_kart(kart)
{
    m_node = NULL;

#ifndef SERVER_ONLY
    if (CVS->isGLSL())
    {
        Material* material =
            material_manager->getMaterialSPM("slipstream.png", "");
        SP::SPMesh* mesh = createMesh(material);
        m_node = irr_driver->addMesh(mesh, "slipstream");
        mesh->drop();
        std::string debug_name = m_kart->getIdent()+" (slip-stream)";
        m_node->setName(debug_name.c_str());
        m_node->setPosition(core::vector3df(0, 0 * 0.25f + 2.5f,
            m_kart->getKartLength()));
        m_node->setVisible(false);
        SP::SPMeshNode* spmn = dynamic_cast<SP::SPMeshNode*>(m_node);
        assert(spmn);
        setSPTM(spmn->getTextureMatrix(0).data());
    }
#endif

    m_slipstream_time      = 0.0f;

    float length = m_kart->getKartProperties()->getSlipstreamLength();
    float kw     = m_kart->getKartWidth();
    float ew     = m_kart->getKartProperties()->getSlipstreamWidth();
    float kl     = m_kart->getKartLength();

    Vec3 p[4];
    p[0]=Vec3(-kw*0.5f, 0, -kl*0.5f       );
    p[1]=Vec3(-ew*0.5f, 0, -kl*0.5f-length);
    p[2]=Vec3( ew*0.5f, 0, -kl*0.5f-length);
    p[3]=Vec3( kw*0.5f, 0, -kl*0.5f       );
    m_slipstream_quad          = new Quad(p[0], p[1], p[2], p[3]);
#ifndef SERVER_ONLY
    if (UserConfigParams::m_slipstream_debug)
    {
        m_debug_dc = std::make_shared<SP::SPDynamicDrawCall>
            (scene::EPT_TRIANGLE_STRIP,
            SP::SPShaderManager::get()->getSPShader("additive"),
            material_manager->getDefaultSPMaterial("additive"));
        m_debug_dc->getVerticesVector().resize(4);
        video::S3DVertexSkinnedMesh* v =
            m_debug_dc->getVerticesVector().data();
        video::SColor red(128, 255, 0, 0);
        unsigned idx[] = { 0, 3, 1, 2 };
        for (unsigned i = 0; i < 4; i++)
        {
            v[i].m_position = p[idx[i]].toIrrVector();
            v[i].m_normal = 0x1FF << 10;
            v[i].m_color = red;
        }
        m_debug_dc->recalculateBoundingBox();
        m_debug_dc->setParent(m_kart->getNode());
        SP::addDynamicDrawCall(m_debug_dc);
    }
#endif
}   // SlipStream

//-----------------------------------------------------------------------------
/** Removes the node from the scene graph.
 */
SlipStream::~SlipStream()
{
    if (m_node)
    {
        irr_driver->removeNode(m_node);
    }
    if (m_debug_dc)
    {
        m_debug_dc->removeFromSP();
    }
    delete m_slipstream_quad;

}   // ~SlipStream

//-----------------------------------------------------------------------------
/** Called at re-start of a race. */
void SlipStream::reset()
{
    m_slipstream_mode = SS_NONE;
    m_slipstream_time = 0;

    // Reset a potential max speed increase
    m_kart->increaseMaxSpeed(MaxSpeed::MS_INCREASE_SLIPSTREAM, 0, 0, 0, 0);
}   // reset

//-----------------------------------------------------------------------------
/** Creates the mesh for the slipstream effect. This function creates a
 *  first a series of circles (with a certain number of vertices each and
 *  distance from each other. Then it will create the triangles and add
 *  texture coordniates.
 *  \param material  The material to use.
 */
SP::SPMesh* SlipStream::createMesh(Material* material)
{
    SP::SPMesh* spm = NULL;
#ifndef SERVER_ONLY
    // All radius, starting with the one closest to the kart (and
    // widest) to the one furthest away. A 0 indicates the end of the list
    float radius[] = {1.5f, 1.0f, 0.5f, 0.0f};

    // The distance of each of the circle from the kart. The number of
    // entries in this array must be the same as the number of non-zero
    // entries in the radius[] array above. No 'end of list' entry required.
    // Note also that in order to avoid a 'bent' in the texture the
    // difference between the distances must be linearly correlated to the
    // difference in the corresponding radius, see the following sideview:
    //            +
    //       +    |
    //  +    |    |    three radius
    //  |    |    |
    //  +----+----+
    //  0    1    2
    //  distances
    // (radius1-radius0)/(distance1-distance0) = (radius2-radius1)/(distnace2-distance0)
    // This way the line connecting the upper "+" is a straight line,
    // and so the 3d cone shape will not be disturbed.
    float distance[] = {2.0f, 6.0f, 10.0f };

    // The alpha values for the rings, no 'end of list' entry required.
    int alphas[]     = {0, 255, 0};

    // Loop through all given radius to determine the number
    // of segments to create.
    unsigned int num_circles=0;
    while(radius[num_circles]>0.0f) num_circles++;

    assert(num_circles > 0);

    // Length is distance of last circle to distance of first circle:
    m_length = distance[num_circles-1] - distance[0];

    // The number of points for each circle. Since part of the slip stream
    // might be under the ground (esp. first and last segment), specify
    // which one is the first and last to be actually drawn.
    const unsigned int  num_segments   = 15;
    const unsigned int  first_segment  = 0;
    const unsigned int  last_segment   = 14;
    const float         f              = 2*M_PI/float(num_segments);
    SP::SPMeshBuffer* buffer           = new SP::SPMeshBuffer();

    static_cast<SP::SPPerObjectUniform*>(buffer)->addAssignerFunction
        ("custom_alpha", [this](SP::SPUniformAssigner* ua)->void
        {
            // In sp shader it's assigned reverse by 1.0 - custom_alpha
            ua->setValue(1.0f - m_slipstream_time);
        });

    std::vector<uint16_t> indices;
    std::vector<video::S3DVertexSkinnedMesh> vertices;
    for(unsigned int j=0; j<num_circles; j++)
    {
        float curr_distance = distance[j]-distance[0];
        // Create the vertices for each of the circle
        for(unsigned int i=first_segment; i<=last_segment; i++)
        {
            video::S3DVertexSkinnedMesh v;
            // Offset every 2nd circle by one half segment to increase
            // the number of planes so it looks better.
            v.m_position.X =  sin((i+(j%2)*0.5f)*f)*radius[j];
            v.m_position.Y = -cos((i+(j%2)*0.5f)*f)*radius[j];
            v.m_position.Z = distance[j];
            // Enable texture matrix and dummy normal for visualization
            v.m_normal = 0x1FF << 10 | 1 << 30;
            v.m_color = video::SColor(alphas[j], 255, 255, 255);
            v.m_all_uvs[0] = MiniGLM::toFloat16(curr_distance/m_length);
            v.m_all_uvs[1] = MiniGLM::toFloat16(
                (float)(i-first_segment)/(last_segment-first_segment)
                + (j%2)*(.5f/num_segments));
            vertices.push_back(v);
        }   // for i<num_segments
    }   // while radius[num_circles]!=0

    // Now create the triangles from circle j to j+1 (so the loop
    // only goes to num_circles-1).
    const int diff_segments = last_segment-first_segment+1;
    for(unsigned int j=0; j<num_circles-1; j++)
    {
        for(unsigned int i=first_segment; i<last_segment; i++)
        {
            indices.push_back(uint16_t( j   *diff_segments+i  ));
            indices.push_back(uint16_t((j+1)*diff_segments+i  ));
            indices.push_back(uint16_t( j   *diff_segments+i+1));
            indices.push_back(uint16_t( j   *diff_segments+i+1));
            indices.push_back(uint16_t((j+1)*diff_segments+i  ));
            indices.push_back(uint16_t((j+1)*diff_segments+i+1));
        }
    }   // for j<num_circles-1
    buffer->setSPMVertices(vertices);
    buffer->setIndices(indices);
    buffer->setSTKMaterial(material);

    spm = new SP::SPMesh();
    spm->addSPMeshBuffer(buffer);
    spm->updateBoundingBox();
#endif
    return spm;
}   // createMesh

//-----------------------------------------------------------------------------
/** Sets the animation intensity (or speed).
 *  \param f Intensity: 0 = no slip stream,
 *                      1 = collecting
 *                      2 = using slip stream bonus
 */
void SlipStream::setIntensity(float f, const AbstractKart *kart)
{
    if (!kart || !m_node)
    {
        if (m_node)
        {
            m_node->setVisible(false);
        }
        return;
    }

    m_node->setVisible(true);
    const float above_terrain = 0.2f;
    core::vector3df my_pos = m_kart->getNode()->getPosition();
    my_pos.Y = m_kart->getHoT()+above_terrain;
    m_node->setPosition(my_pos);

    core::vector3df other_pos = kart->getNode()->getPosition();
    other_pos.Y = kart->getHoT()+above_terrain;
    core::vector3df diff =   other_pos - my_pos;
    core::vector3df rotation = diff.getHorizontalAngle();
    m_node->setRotation(rotation);
    float fs = diff.getLength()/m_length;
    m_node->setScale(core::vector3df(1, 1, fs));

    // For real testing in game: this needs some tuning!
    m_node->setVisible(f!=0);
    MovingTexture::setSpeed(f, 0);

    return;
    // For debugging: make the slip stream effect visible all the time
    m_node->setVisible(true);
    MovingTexture::setSpeed(1.0f, 0.0f);
}   // setIntensity

//-----------------------------------------------------------------------------
/** Returns true if enough slipstream credits have been accumulated
*  to get a boost when leaving the slipstream area.
*/
bool SlipStream::isSlipstreamReady() const
{
    return m_slipstream_time>
        m_kart->getKartProperties()->getSlipstreamCollectTime();
}   // isSlipstreamReady

//-----------------------------------------------------------------------------
/** Returns the additional force being applied to the kart because of
 *  slipstreaming.
 */
void SlipStream::updateSlipstreamPower()
{
    // See if we are currently using accumulated slipstream credits:
    // -------------------------------------------------------------
    if(m_slipstream_mode==SS_USE)
    {
        setIntensity(2.0f, NULL);
        const KartProperties *kp = m_kart->getKartProperties();
        m_kart->increaseMaxSpeed(MaxSpeed::MS_INCREASE_SLIPSTREAM,
                                kp->getSlipstreamMaxSpeedIncrease(),
                                kp->getSlipstreamAddPower(),
                                kp->getSlipstreamDuration(),
                                kp->getSlipstreamFadeOutTime());
    }
}   // upateSlipstreamPower

//-----------------------------------------------------------------------------
/** Sets the color of the debug mesh (which shows the area in which slipstream
 *  can be accumulated).
 *  Color codes:
 *  black:  kart too slow
 *  red:    not inside of slipstream area
 *  green:  slipstream is being accumulated.
 */
void SlipStream::setDebugColor(const video::SColor &color)
{
    if (!m_debug_dc)
    {
        return;
    }

    video::S3DVertexSkinnedMesh* v = m_debug_dc->getVerticesVector().data();
    for (unsigned i = 0; i < 4; i++)
    {
        v[i].m_color = color;
    }
    m_debug_dc->setUpdateOffset(0);

}   // setDebugColor

//-----------------------------------------------------------------------------
/** Update, called once per timestep.
 *  \param dt Time step size.
 */
void SlipStream::update(float dt)
{
    const KartProperties *kp = m_kart->getKartProperties();

    // Low level AIs and ghost karts should not do any slipstreaming.
    if (m_kart->getController()->disableSlipstreamBonus()
        || m_kart->isGhostKart())
        return;

    MovingTexture::update(dt);

    if(m_slipstream_mode==SS_USE)
    {
        m_slipstream_time -= dt;
        if(m_slipstream_time<0) m_slipstream_mode=SS_NONE;
    }

    updateSlipstreamPower();

    // If this kart is too slow for slipstreaming taking effect, do nothing
    // --------------------------------------------------------------------
    // Define this to get slipstream effect shown even when the karts are
    // not moving. This is useful for debugging the graphics of SS-ing.
//#define DISPLAY_SLIPSTREAM_WITH_0_SPEED_FOR_DEBUGGING
#ifndef DISPLAY_SLIPSTREAM_WITH_0_SPEED_FOR_DEBUGGING
    if(m_kart->getSpeed() < kp->getSlipstreamMinSpeed())
    {
        setIntensity(0, NULL);
        m_slipstream_mode = SS_NONE;
        if(UserConfigParams::m_slipstream_debug)
            setDebugColor(video::SColor(255, 0, 0, 0));
        return;
    }
#endif

    // Then test if this kart is in the slipstream range of another kart:
    // ------------------------------------------------------------------
    World *world           = World::getWorld();
    unsigned int num_karts = world->getNumKarts();
    bool is_sstreaming     = false;
    m_target_kart          = NULL;

    // Note that this loop can not be simply replaced with a shorter loop
    // using only the karts with a better position - since a kart might
    // be a lap behind
    for(unsigned int i=0; i<num_karts; i++)
    {
        m_target_kart= world->getKart(i);
        // Don't test for slipstream with itself, a kart that is being
        // rescued or exploding, a ghost kart or an eliminated kart
        if(m_target_kart==m_kart               ||
            m_target_kart->getKartAnimation()  ||
            m_target_kart->isGhostKart()       ||
            m_target_kart->isEliminated()        ) continue;

        // Transform this kart location into target kart point of view
        Vec3 lc = m_target_kart->getTrans().inverse()(m_kart->getXYZ());

        // If the kart is 'on top' of this kart (e.g. up on a bridge),
        // don't consider it for slipstreaming.
        if (fabsf(lc.y()) > 6.0f) continue;

        // If the kart we are testing against is too slow, no need to test
        // slipstreaming. Note: We compare the speed of the other kart
        // against the minimum slipstream speed kart of this kart - not
        // entirely sure if this makes sense, but it makes it easier to
        // give karts different slipstream properties.
#ifndef DISPLAY_SLIPSTREAM_WITH_0_SPEED_FOR_DEBUGGING
        if (m_target_kart->getSpeed() < kp->getSlipstreamMinSpeed())
        {
            if(UserConfigParams::m_slipstream_debug &&
                m_kart->getController()->isLocalPlayerController())
                m_target_kart->getSlipstream()
                              ->setDebugColor(video::SColor(255, 0, 0, 0));

            continue;
        }
#endif
        // Quick test: the kart must be not more than
        // slipstream length+0.5*kart_length()+0.5*target_kart_length
        // away from the other kart
        Vec3 delta = m_kart->getXYZ() - m_target_kart->getXYZ();
        float l    = kp->getSlipstreamLength()
                   + 0.5f*( m_target_kart->getKartLength()
                           +m_kart->getKartLength()        );
        if(delta.length2() > l*l)
        {
            if(UserConfigParams::m_slipstream_debug &&
                m_kart->getController()->isLocalPlayerController())
                m_target_kart->getSlipstream()
                             ->setDebugColor(video::SColor(255, 0, 0, 128));
            continue;
        }
        // Real test: if in slipstream quad of other kart
        if(m_target_kart->getSlipstream()->m_slipstream_quad
                                         ->pointInside(lc))
        {
            is_sstreaming     = true;
            break;
        }
        if(UserConfigParams::m_slipstream_debug &&
            m_kart->getController()->isLocalPlayerController())
            m_target_kart->getSlipstream()
                         ->setDebugColor(video::SColor(255, 0, 0, 255));
    }   // for i < num_karts

    if(!is_sstreaming)
    {
        if(UserConfigParams::m_slipstream_debug &&
            m_kart->getController()->isLocalPlayerController())
            m_target_kart->getSlipstream()
                         ->setDebugColor(video::SColor(255, 255, 0, 0));

        if(isSlipstreamReady())
        {
            // The first time slipstream is ready after collecting
            // and you are leaving the slipstream area, you get a
            // zipper bonus.
            if(m_slipstream_mode==SS_COLLECT)
            {
                m_slipstream_mode = SS_USE;
                m_kart->handleZipper();
                m_slipstream_time = kp->getSlipstreamCollectTime();
                return;
            }
        }
        m_slipstream_time -=dt;
        if(m_slipstream_time<0) m_slipstream_mode = SS_NONE;
        setIntensity(0, NULL);
        return;
    }   // if !is_sstreaming

    if(UserConfigParams::m_slipstream_debug &&
        m_kart->getController()->isLocalPlayerController())
        m_target_kart->getSlipstream()->setDebugColor(video::SColor(255, 0, 255, 0));
    // Accumulate slipstream credits now
    m_slipstream_time = m_slipstream_mode==SS_NONE ? dt
                                                   : m_slipstream_time+dt;
    if(isSlipstreamReady())
        m_kart->setSlipstreamEffect(9.0f);
    setIntensity(m_slipstream_time, m_target_kart);

    m_slipstream_mode = SS_COLLECT;
    if (m_slipstream_time > kp->getSlipstreamCollectTime())
    {
        setIntensity(1.0f, m_target_kart);
    }
}   // update
