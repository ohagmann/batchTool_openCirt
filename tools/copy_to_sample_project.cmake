# Legt das fertige Plugin ins Beispielprojekt und entfernt dort aeltere Staende.
# Aufruf aus CMakeLists.txt nach dem Bauen:
#   cmake -DOC_MODULE=<Datei> -DOC_SAMPLE_DIR=<Ordner> -DOC_SUFFIX=<.brx|.lrx> -P copy_to_sample_project.cmake
if(NOT OC_MODULE OR NOT OC_SAMPLE_DIR OR NOT OC_SUFFIX)
    message(FATAL_ERROR "copy_to_sample_project.cmake: OC_MODULE, OC_SAMPLE_DIR und OC_SUFFIX angeben")
endif()
if(NOT EXISTS "${OC_MODULE}")
    message(FATAL_ERROR "Plugin nicht gefunden: ${OC_MODULE}")
endif()
file(MAKE_DIRECTORY "${OC_SAMPLE_DIR}")
get_filename_component(_name "${OC_MODULE}" NAME)
# Vorhandene Dateien werden geloescht, nicht ueberschrieben: eine Datei, die
# ein laufendes BricsCAD geladen hat, darf nicht an Ort und Stelle veraendert
# werden. Nach dem Loeschen behaelt BricsCAD seinen alten Stand.
file(GLOB _old "${OC_SAMPLE_DIR}/batchtool*${OC_SUFFIX}")
foreach(_f IN LISTS _old)
    get_filename_component(_n "${_f}" NAME)
    file(REMOVE "${_f}")
    if(NOT _n STREQUAL _name)
        message(STATUS "Aelterer Stand entfernt: ${_n}")
    endif()
endforeach()
file(COPY "${OC_MODULE}" DESTINATION "${OC_SAMPLE_DIR}")
message(STATUS "Ins Beispielprojekt gelegt: ${OC_SAMPLE_DIR}/${_name}")
