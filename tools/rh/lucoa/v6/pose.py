# Pose the rigged model (Mixamo bones) for the 5 battle-throw frames and render them from behind.
# Armature space: x = her left, y = up, z = forward.
import bpy, sys, math, os
from mathutils import Vector, Matrix, Quaternion
sys.path.insert(0, 'm3d'); import render as R

def aim(arm, name, d):
    pb = arm.pose.bones['mixamorig:' + name]
    bpy.context.view_layer.update()
    m = pb.matrix.copy()
    cur = (m.to_3x3() @ Vector((0, 1, 0))).normalized()
    q = cur.rotation_difference(Vector(d).normalized())
    rot = q.to_matrix() @ m.to_3x3()
    nm = rot.to_4x4(); nm.translation = m.translation
    pb.matrix = nm
    bpy.context.view_layer.update()

def twist(arm, name, axis, deg):
    """rotate a bone about an armature-space axis (keeps children following)"""
    pb = arm.pose.bones['mixamorig:' + name]
    bpy.context.view_layer.update()
    m = pb.matrix.copy()
    q = Quaternion(Vector(axis), math.radians(deg))
    nm = (q.to_matrix() @ m.to_3x3()).to_4x4(); nm.translation = m.translation
    pb.matrix = nm
    bpy.context.view_layer.update()

DOWN_L = [('LeftArm', (0.22, -1, 0.05)), ('LeftForeArm', (0.08, -1, 0.15)), ('LeftHand', (0.05, -1, 0.2))]
DOWN_R = [('RightArm', (-0.22, -1, 0.05)), ('RightForeArm', (-0.08, -1, 0.15)), ('RightHand', (-0.05, -1, 0.2))]
TW1, TW2, TW3, TW4 = [float(v) for v in os.environ.get('TW', '15,22,-18,-24').split(',')]
POSES = [
    # (spine twist deg about up axis, spine lean fwd deg, left (throwing) arm, right arm)
    (0, 0, DOWN_L, DOWN_R),
    # wind-up: left arm swings down and back
    (TW1, 3, [('LeftArm', (0.55, -0.8, -0.3)), ('LeftForeArm', (0.7, -0.55, -0.45)), ('LeftHand', (0.7, -0.5, -0.45))], DOWN_R),
    # cocked: left arm up and back, ball hand above the shoulder
    (TW2, -2, [('LeftArm', (0.8, -0.5, -0.3)), ('LeftForeArm', (0.2, 1, -0.25)), ('LeftHand', (0.1, 1, -0.15))], DOWN_R),
    # release: left arm sweeps forward and up toward the foe
    (TW3, 8, [('LeftArm', (-0.75, 0.4, 0.55)), ('LeftForeArm', (-0.85, 0.35, 0.4)), ('LeftHand', (-0.85, 0.3, 0.4))],
     [('RightArm', (-0.5, -0.7, -0.45)), ('RightForeArm', (-0.4, -0.7, -0.4)), ('RightHand', (-0.4, -0.7, -0.4))]),
    # follow-through: left arm comes down across in front
    (TW4, 14, [('LeftArm', (-0.65, -0.35, 0.7)), ('LeftForeArm', (-0.8, -0.5, 0.35)), ('LeftHand', (-0.8, -0.55, 0.25))],
     [('RightArm', (-0.45, -0.8, -0.3)), ('RightForeArm', (-0.3, -0.8, -0.2)), ('RightHand', (-0.3, -0.8, -0.2))]),
]

def grow_horns(mesh, k):
    """scale the horn geometry (found by its texture area) about each horn's root"""
    me = mesh.data; uv = me.uv_layers.active.data
    hv = set()
    for poly in me.polygons:
        us = [uv[li].uv for li in poly.loop_indices]
        if all(0.36 <= u.x <= 0.42 and u.y <= 0.27 for u in us): hv.update(poly.vertices)
    for side in (1, -1):
        vs = [me.vertices[i] for i in hv if me.vertices[i].co.x * side > 0]
        if not vs: continue
        # root = the lowest part of the horn (where it meets the cap)
        zs = sorted(v.co.z for v in vs); zcut = zs[len(zs) // 8]
        low = [v.co for v in vs if v.co.z <= zcut]
        root = sum(low, Vector()) / len(low)
        for v in vs: v.co = root + (v.co - root) * k
    print('horn verts', len(hv))

def apply_pose(arm, i):
    for pb in arm.pose.bones:
        pb.rotation_mode = 'QUATERNION'; pb.rotation_quaternion = (1, 0, 0, 0); pb.location = (0, 0, 0)
    bpy.context.view_layer.update()
    tw, lean, la, ra = POSES[i]
    for b in ('Spine', 'Spine1', 'Spine2'):
        twist(arm, b, (0, 1, 0), tw / 3)
        twist(arm, b, (1, 0, 0), lean / 3)
    for n, d in la + ra: aim(arm, n, d)
    # slightly thicker arms so they survive at 64px (scale across the bone, not along it)
    for side in ('Left', 'Right'):
        for b in ('Arm', 'ForeArm'):
            pb = arm.pose.bones['mixamorig:' + side + b]; pb.scale = (1.25, 1.0, 1.25)
            for c in pb.children: c.scale = (1 / 1.25, 1.0, 1 / 1.25)
        arm.pose.bones['mixamorig:' + side + 'Hand'].scale = (1.3, 1.3, 1.3) if i < 3 else (1.0, 1.0, 1.0)
    if i in (1, 2): arm.pose.bones['mixamorig:LeftHand'].scale = (2.0, 2.0, 2.0)   # fist holding the ball
    # throwing hand: fingers curled around the ball
    if True:
        for f in ('Index', 'Middle', 'Ring', 'Pinky'):
            for k in (1, 2, 3):
                pb = arm.pose.bones['mixamorig:LeftHand%s%d' % (f, k)]
                pb.rotation_quaternion = Quaternion((1, 0, 0), math.radians(float(os.environ.get('CURL', 70)) if i in (1, 2) else 35))
        bpy.context.view_layer.update()
    bpy.context.view_layer.update()

if __name__ == '__main__':
    mode = sys.argv[1] if len(sys.argv) > 1 else 'flat'
    mesh, arm = R.setup()
    grow_horns(mesh, float(os.environ.get('HORN', 1.5)))
    sc = bpy.context.scene
    sc.render.resolution_x = sc.render.resolution_y = 512
    apply_pose(arm, 0)
    lo, hi = R.bbox(mesh)
    # fixed camera for all frames: behind her, a little to her left, slightly above
    cam_d = bpy.data.cameras.new('cam'); cam_d.type = 'ORTHO'; cam_d.ortho_scale = 112
    cam = bpy.data.objects.new('cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
    yaw, pit = math.radians(float(os.environ.get('YAW', 140))), math.radians(12)
    target = Vector((0, 0, 151))
    pos = target + Vector((math.sin(yaw) * math.cos(pit), -math.cos(yaw) * math.cos(pit), math.sin(pit))) * 600
    cam.location = pos; cam.rotation_euler = (target - pos).to_track_quat('-Z', 'Y').to_euler(); cam_d.clip_end = 3000
    cam_d.shift_x = float(os.environ.get('SHX', -0.12)); cam_d.shift_y = -0.02
    if mode == 'lit':
        # grey diffuse + sun from the upper left (camera side) for shading levels
        for m in mesh.data.materials:
            nt = m.node_tree; nt.nodes.clear()
            d = nt.nodes.new('ShaderNodeBsdfDiffuse'); d.inputs['Color'].default_value = (0.8, 0.8, 0.8, 1)
            o = nt.nodes.new('ShaderNodeOutputMaterial'); nt.links.new(d.outputs[0], o.inputs[0])
        sun = bpy.data.lights.new('sun', 'SUN'); sun.energy = 3.0
        so = bpy.data.objects.new('sun', sun); sc.collection.objects.link(so)
        so.rotation_euler = (math.radians(45), 0, math.radians(float(os.environ.get('SUNZ', 160))))
        w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True
        w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.35
        sc.cycles.samples = 32
    os.makedirs('m3d/out', exist_ok=True)
    for i in range(5):
        apply_pose(arm, i)
        R.render(f'm3d/out/{mode}_{i}.png')
