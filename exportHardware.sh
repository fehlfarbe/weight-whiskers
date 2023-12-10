#!/bin/bash

HARDWARE="./hardware/"
EXPORT=${HARDWARE}/export/
GERBER=${EXPORT}/gerbers/
DOC="./doc/"
PCB=${HARDWARE}/cat_scale.kicad_pcb
SCH=${HARDWARE}/cat_scale.kicad_sch

mkdir -p ${GERBER}
mkdir -p ${EXPORT}

# gerbers and drill files
kicad-cli pcb export gerbers --board-plot-params ${PCB} -o ${GERBER}/
kicad-cli pcb export drill --map-format gerberx2 ${PCB} -o ${GERBER}/

# BOM
kicad-cli sch export python-bom ${SCH} -o ${EXPORT}/BOM.xml

# 3d step model
kicad-cli pcb export step --subst-models  ${PCB} -o ${EXPORT}/weight-whiskers.step

# images for doc
kicad-cli sch export svg ${SCH} -o ${DOC}
kicad-cli sch export pdf ${SCH} -o ${DOC}/pcb_schematic.pdf
pcbdraw plot --no-components --side front ${PCB} ${DOC}/pcb_front.png
pcbdraw plot --no-components --side back ${PCB} ${DOC}/pcb_back.png
pcbdraw render --side front --renderer raytrace ${PCB} ${DOC}/pcb_render_front.png
pcbdraw render --side back --renderer raytrace ${PCB} ${DOC}/pcb_render_back.png