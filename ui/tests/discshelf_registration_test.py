#!/usr/bin/env python3
"""Preserve installed-app IDs, shortcuts and menu ordering while introducing a builtin."""
from pathlib import Path
import tempfile,subprocess,os
ui=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 defs=['-D'+name+'='+name.replace('simple_','') for name in ['simple_cfg_get_int','simple_cfg_get_str','simple_cfg_set_int','simple_cfg_set_str','simple_cfg_set_int_deferred','simple_cfg_set_str_deferred']]
 defs += ['-DCFG_PATH="'+str(p/'diskos.conf')+'"','-DCFG_DIR="'+str(p)+'"','-DLEGACY_CONF="'+str(p/'none')+'"','-DLEGACY_SWIPE="'+str(p/'no-swipe')+'"','-DMENU_APPS_DIR="'+str(p/'apps')+'"']
 for name in ['menu_apps','apps_shortcut']:
  sources=[ui/'tests'/(name+'_test.c'),ui/'config.c',ui/'md5.c']
  if name=='menu_apps':sources += [ui/'menu_apps.c']
  out=p/name
  subprocess.run(['cc','-g','-std=gnu11','-D_GNU_SOURCE','-DLV_CONF_INCLUDE_SIMPLE','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-I'+str(ui),*defs,*map(str,sources),'-Wl,--gc-sections','-o',str(out)],check=True)
  subprocess.run([str(out)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
