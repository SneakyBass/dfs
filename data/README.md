# Data

You can download the maps json files from the [Releases](REDACTED)
and extract the `maps` folder to the `data` folder on your filesystem.

You will also need the worldgraph to move between maps.

## Manually getting maps data

Get UABEA from the [releases page](https://github.com/nesrak1/UABEA/releases)

Then copy the maps to some directory

```bash
mkdir maps
cp Dofus-dofus3/Dofus_Data/StreamingAssets/Content/Map/Data/*.bundle maps/
```

Export the bundles

```bash
./UABEAvalonia batchexportbundle maps/ -md
```

Run UABEA and go to File > Open and select all the newly created files.

Ctrl-A then click `Export Dump` then choose json dump.

Export everything to a new `maps-json` folder.

This may take some time as there are many maps.

You will also need to get the worldgraph. Open the bundle located in
`Dofus-dofus3/Dofus_Data/StreamingAssets/aa/StandaloneLinux64/worldassets_assets_all_<UID>.bundle`
and export the worldgraph into the `worldgraph.json` file.

## Generating map data

Move the `maps-json` folder and the `worldgraph.json` file into `data/`.

```bash
mv <EXPORT_DIR>/maps-json data/maps-json
mv <EXPORT_DIR>/worldgraph.json data/
```

Then just run the program. The first run should generate all of the maps
under the `data/maps` folder.

You can now remove the `maps-json` folder to avoid regenerating maps on each run.
