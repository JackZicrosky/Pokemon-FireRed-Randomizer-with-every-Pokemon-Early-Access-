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
TW1, TW2, TW3, TW4 = [float(v) for v in os.environ.get('TW', '15,22,-50,-55').split(',')]
LEAN = [float(v) for v in os.environ.get('LEAN', '10,13,8,18,24').split(',')]
POSES = [
    # (spine twist deg about up axis, spine lean fwd deg, left (throwing) arm, right arm like Red's other arm)
    (0, LEAN[0], DOWN_L, DOWN_R),
    # wind-up: left arm down, forearm up to the hand; right arm swung out down-right
    (TW1, LEAN[1], [('LeftArm', (0.4, -0.9, -0.2)), ('LeftForeArm', (0.5, 0.55, -0.55)), ('LeftHand', (0.45, 0.7, -0.45))],
     [('RightArm', (-0.5, -0.85, 0.1)), ('RightForeArm', (-0.35, -0.9, 0.25)), ('RightHand', (-0.3, -0.9, 0.3))]),
    # cocked: elbow out, fist beside her head; right arm down, a little out
    (TW2, LEAN[2], [('LeftArm', (0.8, -0.5, -0.3)), ('LeftForeArm', (0.2, 1, -0.25)), ('LeftHand', (0.1, 1, -0.15))],
     [('RightArm', (-0.4, -0.9, 0.05)), ('RightForeArm', (-0.35, -0.9, 0.15)), ('RightHand', (-0.35, -0.9, 0.15))]),
    # release: left arm sweeps toward the foe; right arm pulled back and bent, hand by the far hip
    (TW3, LEAN[3], [('LeftArm', (float(os.environ.get('AX',-0.2)), float(os.environ.get('AY',0.3)), 0.9)), ('LeftForeArm', (float(os.environ.get('AX',-0.2)) - 0.1, float(os.environ.get('FY',0.05)), 0.9)), ('LeftHand', (float(os.environ.get('AX',-0.2)) - 0.1, float(os.environ.get('FY',0.05)), 0.9))],
     [('RightArm', (-0.3, -0.9, -0.35)), ('RightForeArm', (-0.1, -0.95, 0.2)), ('RightHand', (-0.1, -0.95, 0.25))]),
    # follow-through: left arm down across in front; right arm still back and bent
    (TW4, LEAN[4], [('LeftArm', (-0.15, -0.45, 0.85)), ('LeftForeArm', (-0.25, -0.55, 0.75)), ('LeftHand', (-0.25, -0.6, 0.7))],
     [('RightArm', (-0.3, -0.9, -0.3)), ('RightForeArm', (-0.1, -0.95, 0.2)), ('RightHand', (-0.1, -0.95, 0.25))]),
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
        arm.pose.bones['mixamorig:' + side + 'Hand'].scale = (1.0, 1.0, 1.0)
    if i == 2:   # raised forearm in frame 3: thicker so the wrist matches the hand
        k = 1.7; pb = arm.pose.bones['mixamorig:LeftForeArm']; pb.scale = (k, 1.0, k)
        arm.pose.bones['mixamorig:LeftHand'].scale = (1 / k, 1.0, 1 / k)
    # (frames 2-3 get a hand-drawn fist in convert.py)
    # throwing hand: fingers curled around the ball
    if True:
        for f in ('Index', 'Middle', 'Ring', 'Pinky'):
            for k in (1, 2, 3):
                pb = arm.pose.bones['mixamorig:LeftHand%s%d' % (f, k)]
                pb.rotation_quaternion = Quaternion((1, 0, 0), math.radians(float(os.environ.get('CURL', 55)) if i in (1, 2) else 35))
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
    yaw, pit = math.radians(float(os.environ.get('YAW', 215))), math.radians(12)
    target = Vector((0, 0, 151))
    pos = target + Vector((math.sin(yaw) * math.cos(pit), -math.cos(yaw) * math.cos(pit), math.sin(pit))) * 600
    cam.location = pos; cam.rotation_euler = (target - pos).to_track_quat('-Z', 'Y').to_euler(); cam_d.clip_end = 3000
    cam_d.shift_x = float(os.environ.get('SHX', -0.06)); cam_d.shift_y = -0.02
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
        sc.cycles.samples = 64
        # smooth shading + a weaker fill light from her front right, so the bust and arms get rounded form
        for poly in mesh.data.polygons: poly.use_smooth = True
        fill = bpy.data.lights.new('fill', 'SUN'); fill.energy = float(os.environ.get('FILL', 1.6))
        fo = bpy.data.objects.new('fill', fill); sc.collection.objects.link(fo)
        fo.rotation_euler = (math.radians(60), 0, math.radians(float(os.environ.get('FILLZ', 330))))
    os.makedirs('m3d/out', exist_ok=True)
    from bpy_extras.object_utils import world_to_camera_view
    import json
    hands = {}
    for i in range(5):
        apply_pose(arm, i)
        R.render(f'm3d/out/{mode}_{i}.png')
        # where the throwing hand is in the 64x64 sprite (wrist and knuckles), for the hand-drawn fist
        def px(bone):
            co = arm.matrix_world @ arm.pose.bones['mixamorig:' + bone].head
            v = world_to_camera_view(sc, cam, co); return [v.x * 64, (1 - v.y) * 64]
        hands[i] = {'wrist': px('LeftHand'), 'knuckle': px('LeftHandMiddle2')}
    json.dump(hands, open('m3d/out/hands.json', 'w'))
