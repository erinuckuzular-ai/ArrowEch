# dmgbuild settings for the ArrowEch disk image (see scripts/make-dmg.sh).
# Icon positions must line up with the art from scripts/make-art.py.
import os

stage = defines["stage"]
art = defines["art"]

format = "UDZO"
filesystem = "HFS+"
files = [os.path.join(stage, "Install ArrowEch.pkg"), os.path.join(stage, "READ ME FIRST.txt")]
hide_extensions = ["Install ArrowEch.pkg"]
icon = os.path.join(art, "ArrowEch.icns")

background = os.path.join(art, "dmg-background.tiff")
# Height includes the ~32pt title bar.
window_rect = ((200, 140), (660, 452))
default_view = "icon-view"
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
arrange_by = None
icon_size = 96
text_size = 13
icon_locations = {
    "Install ArrowEch.pkg": (470, 205),
    "READ ME FIRST.txt": (590, 345),
}
