# Probestueck Antennenloch: Ausschnitt aus stl/Rahmen_v1.1.stl um das D-Loch (Y20/Z100),
# Drucklage: Aussenseite (X148) unten, Lochachse senkrecht.
import struct,numpy as np,manifold3d as m3,os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..'))
from rahmen_v1_1_io import load,save
r=load('stl/Rahmen_v1.1.stl')
box=m3.Manifold.cube([12,24,24]).translate([137.0,8.0,88.0])
p=r^box
print('Probe Volumen %.1f'%p.volume(), 'bbox', np.round(p.bounding_box(),2))
# Aussenseite nach unten: Drehung um Y +90 Grad macht z' = -x, die Seite X148 liegt dann unten
p=p.rotate([0,90,0])
b=np.array(p.bounding_box()); p=p.translate([-b[0],-b[1],-b[2]])
print('Drucklage bbox', np.round(p.bounding_box(),2))
save(p,'stl/Probe_Antennenloch_v1.0.stl')
