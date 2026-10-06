# Render the owner's rigged Lucoa model: flat textured (shadeless) or toon-lit, orthographic camera.
import bpy, sys, math, json
from mathutils import Vector, Euler
sys.path.insert(0, 'm3d'); import fbxload
MODEL = 'model/01- Quetzalcoatl Lucoa/Lucoa.fbx'
TEX = 'model/01- Quetzalcoatl Lucoa/texture.png'

def setup(engine='CYCLES'):
    fbxload.load(MODEL)
    for o in list(bpy.data.objects):
        if o.type == 'LIGHT': bpy.data.objects.remove(o)
    mesh = [o for o in bpy.data.objects if o.type == 'MESH'][0]
    arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
    arm.animation_data_clear()           # the file carries a Mixamo take that would override our poses
    # flat material: texture straight to emission (anime-style flat colours)
    img = bpy.data.images.load(bpy.path.abspath('//' + TEX) if False else __import__('os').path.abspath(TEX))
    for m in mesh.data.materials:
        m.use_nodes = True
        nt = m.node_tree; nt.nodes.clear()
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = img; t.interpolation = 'Closest'
        e = nt.nodes.new('ShaderNodeEmission'); o = nt.nodes.new('ShaderNodeOutputMaterial')
        nt.links.new(t.outputs['Color'], e.inputs['Color']); nt.links.new(e.outputs['Emission'], o.inputs['Surface'])
    sc = bpy.context.scene
    sc.render.engine = engine
    if engine == 'CYCLES':
        sc.cycles.samples = 16; sc.cycles.device = 'CPU'; sc.cycles.use_denoising = False
    sc.view_settings.view_transform = 'Standard'
    sc.render.film_transparent = True
    sc.render.resolution_x = 600; sc.render.resolution_y = 800
    return mesh, arm

def bbox(mesh):
    dg = bpy.context.evaluated_depsgraph_get(); ev = mesh.evaluated_get(dg)
    pts = [ev.matrix_world @ v.co for v in ev.data.vertices]
    lo = Vector([min(p[i] for p in pts) for i in range(3)]); hi = Vector([max(p[i] for p in pts) for i in range(3)])
    return lo, hi

def camera(mesh, yaw_deg, pitch_deg=0, frac=(0.0, 1.0), pad=1.05):
    lo, hi = bbox(mesh)
    c = (lo + hi) / 2; h = hi.z - lo.z
    z0, z1 = lo.z + h * frac[0], lo.z + h * frac[1]
    target = Vector((c.x, c.y, (z0 + z1) / 2))
    cam_d = bpy.data.cameras.new('cam'); cam_d.type = 'ORTHO'
    cam_d.ortho_scale = (z1 - z0) * pad * max(1, bpy.context.scene.render.resolution_y / bpy.context.scene.render.resolution_x) / (bpy.context.scene.render.resolution_y / bpy.context.scene.render.resolution_x)
    cam = bpy.data.objects.new('cam', cam_d); bpy.context.scene.collection.objects.link(cam)
    yaw = math.radians(yaw_deg); pit = math.radians(pitch_deg); dist = h * 3
    pos = target + Vector((math.sin(yaw) * math.cos(pit), -math.cos(yaw) * math.cos(pit), math.sin(pit))) * dist
    cam.location = pos
    d = target - pos
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    cam_d.clip_end = dist * 4
    bpy.context.scene.camera = cam
    return cam

def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)

if __name__ == '__main__':
    mesh, arm = setup()
    print('dims', mesh.dimensions, 'rot', mesh.rotation_euler, arm.rotation_euler, arm.scale)
    lo, hi = bbox(mesh); print('bbox', lo, hi)
    for yaw in (0, 180):
        cam = camera(mesh, yaw)
        render(f'm3d/test_{yaw}.png')
        bpy.data.objects.remove(cam)
