"""Synthetic tests; no original game material is used."""
import sys,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from rom_edit_sources import apply_patch,sha
class PixelEdits(unittest.TestCase):
 def setUp(self):
  self.base=np.array([[1,2],[3,4]],np.uint8);self.after=np.array([[1,8],[3,4]],np.uint8)
  self.patch={'shape':[2,2],'base_sha256':sha(self.base),'result_sha256':sha(self.after),'changes':[[1,8]]}
 def test_exact_reconstruction(self):self.assertTrue(np.array_equal(apply_patch(self.base,self.patch),self.after))
 def test_wrong_source_rejected(self):
  with self.assertRaises(ValueError):apply_patch(self.base+1,self.patch)
 def test_wrong_result_rejected(self):
  with self.assertRaises(ValueError):apply_patch(self.base,{**self.patch,'changes':[[1,9]]})
 def test_duplicate_and_out_of_bounds_rejected(self):
  for edits in ([[1,8],[1,8]],[[-1,8]],[[4,8]],[[1,16]]):
   with self.assertRaises(ValueError):apply_patch(self.base,{**self.patch,'changes':edits})
if __name__=='__main__':unittest.main()
