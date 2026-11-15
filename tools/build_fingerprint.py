"""One wire-compatibility fingerprint for group and cartridge builds."""
import hashlib,re
def build_fingerprint(root,units):
 inputs=set(units+['src/startup.S','src/linker.ld','tools/build_native.py','VERSION'])
 inputs.update(str(p.relative_to(root))for folder in ['src','generated']for pattern in ['*.h','*.inc']for p in (root/folder).glob(pattern)if p.name!='native-link-build.h')
 for unit in list(inputs):
  if unit.endswith('.S'):inputs.update(re.findall(r'\.incbin\s+"([^"]+)"',(root/unit).read_text()))
 digest=hashlib.sha256()
 for unit in sorted(inputs):digest.update(unit.encode()+b'\0');digest.update((root/unit).read_bytes())
 fingerprint=digest.digest()
 (root/'generated/native-link-build.h').write_text('#ifndef NATIVE_LINK_BUILD_H\n#define NATIVE_LINK_BUILD_H\n#define NATIVE_LINK_BUILD {'+','.join(str(int.from_bytes(fingerprint[i:i+4],'little'))+'u'for i in range(0,16,4))+'}\n#endif\n')
 return fingerprint
