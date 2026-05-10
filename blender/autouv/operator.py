import bpy
import numpy as np

from . import cache, wrapper


def calculate_metrics(obj):
    mesh = obj.data
    if not mesh.uv_layers: return 0.0, 0.0, 0.0
        
    num_tris = len(mesh.loop_triangles)
    loops = np.zeros(num_tris * 3, dtype=np.int32)
    mesh.loop_triangles.foreach_get("loops", loops)
    
    uv_layer = mesh.uv_layers.active.data
    uvs = np.zeros(num_tris * 3 * 2, dtype=np.float32)
    uv_layer.foreach_get("uv", uvs)
    uvs = uvs.reshape(num_tris, 3, 2)
    
    verts = np.zeros(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", verts)
    verts = verts.reshape(-1, 3)
    
    loop_vert_indices = np.zeros(len(mesh.loops), dtype=np.int32)
    mesh.loops.foreach_get("vertex_index", loop_vert_indices)
    
    tri_coords = verts[loop_vert_indices[loops.reshape(num_tris, 3)]]
    
    u, v = uvs[:, :, 0], uvs[:, :, 1]
    area_2d = 0.5 * np.abs(u[:,0]*(v[:,1]-v[:,2]) + u[:,1]*(v[:,2]-v[:,0]) + u[:,2]*(v[:,0]-v[:,1]))
    total_uv_area = np.sum(area_2d)
    coverage = min(total_uv_area, 1.0)

    e1_3d = tri_coords[:, 1] - tri_coords[:, 0]
    e2_3d = tri_coords[:, 2] - tri_coords[:, 0]
    cross = np.cross(e1_3d, e2_3d)
    area_3d = 0.5 * np.linalg.norm(cross, axis=1)
    
    # Filter degenerate triangles (Area > epsilon)
    valid_mask = area_3d > 1e-6 
    
    if np.sum(valid_mask) > 0:
        total_valid_3d_area = np.sum(area_3d[valid_mask])
        
        if total_valid_3d_area == 0:
             final_stretch = 1.0
        else:
            global_scale = total_uv_area / total_valid_3d_area
            local_scale = area_2d[valid_mask] / area_3d[valid_mask]
            
            if global_scale < 1e-8:
                final_stretch = 1.0
            else:
                stretch_scores = local_scale / global_scale
                stretch_scores[stretch_scores < 1e-6] = 1e-6
                
                # Raw stretch calculation
                raw_metric = np.maximum(stretch_scores, 1.0 / stretch_scores)
                
                # SAFETY CLAMP: Force scores to stay in sanity range [1.0, 5.0]
                # This prevents degenerate triangles from returning 5,000,000
                clamped_metric = np.clip(raw_metric, 1.0, 5.0)
                
                final_stretch = np.median(clamped_metric)
    else:
        final_stretch = 1.0

    return final_stretch, coverage, 0.0

def run_unwrap_logic(operator, context, obj):
    scene = context.scene
    angle = getattr(operator, "angle_limit", 45.0)
    margin = getattr(operator, "margin", 0.02)
    
    params = {
        'angle_threshold': angle,
        'island_margin': margin,
        'min_island_faces': 5,
        'pack_islands': True
    }

    cached_uvs, cached_meta = None, None
    if hasattr(cache, 'get_cached_result'):
        cached_uvs, cached_meta = cache.get_cached_result(obj.data, params)
    
    if cached_uvs is not None:
        operator.report({'INFO'}, f"{obj.name}: Restored from Cache")
        if not obj.data.uv_layers: obj.data.uv_layers.new(name="AutoUV")
        try:
            obj.data.uv_layers.active.data.foreach_set("uv", cached_uvs)
        except Exception:
            pass
            
        if cached_meta:
            scene['autouv_score_stretch'] = float(cached_meta.get('stretch', 1.0))
            scene['autouv_score_coverage'] = float(cached_meta.get('coverage', 0.0))
            scene['autouv_score_angle'] = float(cached_meta.get('angle', 0.0))
        return {'FINISHED'}

    status = wrapper.unwrap_object(obj, angle, margin)
    
    if 'FINISHED' in status:
        stretch, coverage, angle_dist = calculate_metrics(obj)
        
        scene['autouv_score_stretch'] = float(stretch)
        scene['autouv_score_coverage'] = float(coverage)
        scene['autouv_score_angle'] = float(angle_dist)
        
        if hasattr(cache, 'store_cached_result'):
            uv_layer = obj.data.uv_layers.active.data
            uvs = [0.0] * (len(uv_layer) * 2)
            uv_layer.foreach_get("uv", uvs)
            meta = {'stretch': stretch, 'coverage': coverage, 'angle': angle_dist}
            cache.store_cached_result(obj.data, params, uvs, meta)
            
        operator.report({'INFO'}, f"Unwrapped {obj.name}")
        return {'FINISHED'}
    else:
        operator.report({'ERROR'}, f"Failed to unwrap {obj.name}")
        return {'CANCELLED'}

class AutoUV_OT_Unwrap(bpy.types.Operator):
    bl_idname = "autouv.unwrap"
    bl_label = "Auto UV Unwrap"
    bl_options = {'REGISTER', 'UNDO'}

    angle_limit: bpy.props.FloatProperty(name="Angle Threshold", default=45.0, min=1.0, max=180.0)
    margin: bpy.props.FloatProperty(name="Margin", default=0.02, min=0.001, max=0.5)

    def execute(self, context):
        obj = context.active_object
        if not obj or obj.type != 'MESH':
            self.report({'ERROR'}, "Select a mesh object")
            return {'CANCELLED'}
        if hasattr(cache, 'invalidate'): cache.invalidate(obj.data)
        return run_unwrap_logic(self, context, obj)

class AutoUV_OT_BatchUnwrap(bpy.types.Operator):
    bl_idname = "autouv.batch_unwrap"
    bl_label = "Batch Unwrap"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        selected = [o for o in context.selected_objects if o.type == 'MESH']
        if not selected:
            self.report({'WARNING'}, "No meshes selected")
            return {'CANCELLED'}
        wm = context.window_manager
        wm.progress_begin(0, len(selected))
        for i, obj in enumerate(selected):
            wm.progress_update(i)
            if hasattr(cache, 'invalidate'): cache.invalidate(obj.data)
            op_mock = type('obj', (object,), {'report': self.report, 'angle_limit': 45.0, 'margin': 0.02})
            run_unwrap_logic(op_mock, context, obj)
        wm.progress_end()
        self.report({'INFO'}, f"Batch Processed {len(selected)} objects")
        return {'FINISHED'}

class AutoUV_OT_MarkSeams(bpy.types.Operator):
    bl_idname = "autouv.mark_seam"
    bl_label = "Mark Seam"
    bl_options = {'REGISTER', 'UNDO'}
    def execute(self, context):
        bpy.ops.mesh.mark_seam(clear=False)
        obj = context.active_object
        if obj and hasattr(cache, 'invalidate'): cache.invalidate(obj.data)
        return {'FINISHED'}

class AutoUV_OT_ClearSeams(bpy.types.Operator):
    bl_idname = "autouv.clear_seam"
    bl_label = "Clear Seam"
    bl_options = {'REGISTER', 'UNDO'}
    def execute(self, context):
        bpy.ops.mesh.mark_seam(clear=True)
        obj = context.active_object
        if obj and hasattr(cache, 'invalidate'): cache.invalidate(obj.data)
        return {'FINISHED'}

classes = (AutoUV_OT_Unwrap, AutoUV_OT_BatchUnwrap, AutoUV_OT_MarkSeams, AutoUV_OT_ClearSeams)

def register():
    for cls in classes: bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes): bpy.utils.unregister_class(cls)