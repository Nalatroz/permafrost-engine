/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2018 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#version 330 core

#define MAX_JOINTS 96

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in int  in_material_idx;
layout (location = 4) in mat2x3 in_joint_indices; /* 2x3 mat to alias array of 6 floats */
layout (location = 6) in mat2x3 in_joint_weights; /* 2x3 mat to alias array of 6 floats */

/*****************************************************************************/
/* OUTPUTS                                                                   */
/*****************************************************************************/

out VertexToFrag {
         vec2 uv;
    flat int  mat_idx;
         vec3 world_pos;
         vec3 normal;
         vec4 light_space_pos;
}to_fragment;

out VertexToGeo {
    vec3 normal;
}to_geometry;

/*****************************************************************************/
/* UNIFORMS                                                                  */
/*****************************************************************************/

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 light_space_transform;

uniform mat4 anim_curr_pose_mats[MAX_JOINTS];
uniform mat4 anim_inv_bind_mats [MAX_JOINTS];

/*****************************************************************************/
/* PROGRAM
/*****************************************************************************/

void main()
{
    to_fragment.uv = in_uv;
    to_fragment.mat_idx = in_material_idx;

    /* TODO: compute normal matrix on CPU once per model each frame and pass as uniform 
     */
    mat3 normal_matrix_geo = mat3(transpose(inverse(view * model)));
    mat3 normal_matrix = mat3(transpose(inverse(model)));

    float tot_weight = in_joint_weights[0][0] + in_joint_weights[0][1] + in_joint_weights[0][2]
                     + in_joint_weights[1][0] + in_joint_weights[1][1] + in_joint_weights[1][2];

    /* If all weights are 0, treat this vertex as a static one.
     * Non-animated vertices will have their weights explicitly zeroed out. 
     */
    if(tot_weight == 0.0) {

        to_geometry.normal = normalize(vec3(projection * vec4(normal_matrix_geo * in_normal, 1.0)));

        to_fragment.normal = normalize(normal_matrix * in_normal);
        to_fragment.world_pos = (model * vec4(in_pos, 1.0)).xyz;
        to_fragment.light_space_pos = light_space_transform * vec4(to_fragment.world_pos, 1.0);

        gl_Position = projection * view * model * vec4(in_pos, 1.0);

    }else {

        vec3 new_pos =  vec3(0.0, 0.0, 0.0);
        vec3 new_normal = vec3(0.0, 0.0, 0.0);

        for(int w_idx = 0; w_idx < 6; w_idx++) {

            int r = w_idx / 3;
            int c = w_idx % 3;

            int joint_idx = int(in_joint_indices[r][c]);

            mat4 inv_bind_mat = anim_inv_bind_mats [joint_idx];
            mat4 pose_mat     = anim_curr_pose_mats[joint_idx];

            float fraction = in_joint_weights[r][c] / tot_weight;

            mat4 bone_mat = fraction * pose_mat * inv_bind_mat;
            /* Should calculate the rot mat on the CPU as well... */
            mat3 rot_mat = fraction * mat3(transpose(inverse(pose_mat * inv_bind_mat)));
            
            new_pos += (bone_mat * vec4(in_pos, 1.0)).xyz;
            new_normal += rot_mat * in_normal;
        }

        to_geometry.normal = normalize(normal_matrix_geo * new_normal);

        to_fragment.normal = normalize(normal_matrix * new_normal);
        to_fragment.world_pos = (model * vec4(new_pos, 1.0)).xyz;
        to_fragment.light_space_pos = light_space_transform * vec4(to_fragment.world_pos, 1.0);

        gl_Position = projection * view * model * vec4(new_pos, 1.0f);

    }
}

