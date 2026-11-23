# Platinenadapter: SolidCraft-Export (Baugruppenlage) -> Drucklage + Kantenprüfung
# 3. Argument 'vorn': Vorderseite (kleines Y, Streifen v1.1) unten, sonst Auflage Y59 unten (Leiste v1.0)
import sys,struct,collections,numpy as np
src,dst=sys.argv[1],sys.argv[2]
vorn=len(sys.argv)>3 and sys.argv[3]=='vorn'
d=open(src,'rb').read(); n=struct.unpack('<I',d[80:84])[0]
rec=np.frombuffer(d[84:84+n*50],dtype=np.dtype([('n','<3f4'),('v','<9f4'),('a','<u2')])).copy()
v=rec['v'].reshape(-1,3).astype(float)
v=np.stack([v[:,0],-v[:,2],v[:,1]],1) if vorn else np.stack([v[:,0],v[:,2],59-v[:,1]],1)
v[:,2]-=v[:,2].min()
t=v.reshape(-1,3,3); nrm=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]); nrm/=np.linalg.norm(nrm,axis=1,keepdims=True)+1e-12
rec['v']=t.reshape(-1,9); rec['n']=nrm
open(dst,'wb').write(b'\0'*80+struct.pack('<I',n)+rec.tobytes())
_,inv=np.unique(np.round(t/1e-4).astype(np.int64).reshape(-1,3),axis=0,return_inverse=True)
e=collections.Counter()
for a,b,c in inv.reshape(-1,3):
    for u,w in ((a,b),(b,c),(c,a)): e[(u,w)]+=1
bad=sum(1 for (u,w),c in e.items() if c!=1 or e.get((w,u),0)!=1)
vol=np.einsum('ij,ij->i',t[:,0],np.cross(t[:,1],t[:,2])).sum()/6
print(dst.split('/')[-1],n,'Dreiecke, Volumen',round(vol,1),'Groesse',(v.max(0)-v.min(0)).round(2),'offene Kanten',bad)
