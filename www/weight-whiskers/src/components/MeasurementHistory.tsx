import React, { ChangeEvent, useEffect, useState } from "react";
import { Datum, ResponsiveLine, Serie } from '@nivo/line'
import Papa from "papaparse";
import { LoadingImage } from "./Loading";

export interface Measurement {
  timestamp: number;
  weight: number;
}

export interface MeasurementCSV {
  time: number;
  weight: number;
  std: number;
  duration: number;
  weightDropping: number;
}

class Point implements Datum {
  id: number = 0;
  x: string = "";
  y: number = 0;
  selected: boolean = false;
  rawData: MeasurementCSV | undefined = undefined;
}

class MeasurementData implements Serie {
  id: string = 'series';
  data: Array<Point> = [];
}

enum MeasurementFilter {
  LastMonth = "Last month",
  AllData = "All data"
}


const MeasurementHistory = () => {
  const [dataFilter, setDataFilter] = useState<MeasurementFilter>(MeasurementFilter.LastMonth);
  const [filteredData, setFilteredData] = useState<Array<MeasurementData>>([new MeasurementData()]);
  const [allData, setAllData] = useState<Array<MeasurementData>>([new MeasurementData()]);
  const commonConfig = { delimiter: ",", dynamicTyping: true };

  // update measurements filter via dropdown
  const updateMeasurementsFilter = (event: ChangeEvent<HTMLSelectElement>) => {
    // console.log("filtering...", event, dataFilter);
    // setFilteredData(dataHistory);
    setDataFilter(event.target.value as MeasurementFilter);
  }

  // filter data
  const filterMeasurements = () => {
    var measurements = new MeasurementData();

    switch (dataFilter) {
      case MeasurementFilter.AllData:
        console.log("all data");
        measurements = allData[0];
        break;
      case MeasurementFilter.LastMonth:
        console.log("last month");
        var startDate = new Date();
        startDate.setMonth(startDate.getMonth() - 1);
        measurements.data = allData[0].data.filter(d => new Date(d.x) > startDate);
        measurements.id = allData[0].id;
        break;
    }

    // update data
    setFilteredData([measurements]);
  }

  // on click table element
  const selectPoint = (event: React.MouseEvent<HTMLTableRowElement>, id: number) => {
    event.preventDefault();
    var history = filteredData[0];
    var idx = history.data.findIndex(item => item.id == id);
    history.data[idx].selected = !history.data[idx].selected;
    setFilteredData([
      { ...history, data: history.data }
    ]
    );
    console.log(filteredData);
  }

  // send delete request to delete selected elements identified by timestamp
  const deleteSelectedPoints = (event: React.MouseEvent<HTMLButtonElement>) => {
    event.preventDefault();
    var pointsToDelete = filteredData[0].data.filter(d => d.selected);
    var deletePayload = {
      action: "delete",
      timestamps: pointsToDelete.map(point => point.rawData?.time)
    };

    fetch("/api/measurements", {
      method: "POST",
      headers: {
        "Accept": "application/json",
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: JSON.stringify(deletePayload),

    })
      .then((response) => {
        console.log(response.status);
        if (response.status == 200) {
          setAllData([
            {
              ...allData[0],
              data: allData[0].data.filter(d => !d.selected)
            }
          ]
          );
        }
      })
      .catch((error) => {
        console.log("Error");
      });
  }

  // draw std dev hose around values
  const StdDevLine = (props: any) => {
    const { series, xScale, yScale } = props;

    // Draw standard deviation lines
    // Create an area layer to represent the "hose"
    const areaData = series[0].data.map((point: any) => ({
      x: xScale(point.data.x),
      y0: yScale(point.data.y - point.data.rawData.std / 2), // Lower bound
      y1: yScale(point.data.y + point.data.rawData.std / 2), // Upper bound
    }));

    return (
      <path
        d={`M${areaData.map((d: any) => `${d.x},${d.y1}`).join('L')}L${areaData
          .slice()
          .reverse()
          .map((d: any) => `${d.x},${d.y0}`)
          .join('L')}Z`}
        fill="rgba(255, 0, 0, 0.2)" // Adjust color and opacity as needed
      />
    );
  };

  const SelectedMeasurements = (props: any) => {
    const { series, xScale, yScale, innerHeight } = props;
    const areaData = series[0].data.filter((d: any) => d.data.selected).map((point: any) => ({
      x: xScale(point.data.x),
    }));

    return areaData.map((p: any) => (
      (<line
        key={p.x}
        x1={p.x}
        y1={0}
        x2={p.x}
        y2={innerHeight}
        stroke="rgba(255, 0, 0, 1)"
        strokeWidth={10}
      />)
    ));
  };

  // load all measurements
  useEffect(() => {
    if (allData[0].data.length == 0) {
      // load data
      Papa.parse(
        "/api/measurements",
        {
          ...commonConfig,
          header: true,
          download: true,
          complete: (result: Papa.ParseResult<MeasurementCSV>) => {
            console.log("Parsed CSV data", result);
            let measurementData = new MeasurementData();
            measurementData.id = "Measured weight";
            result.data.forEach((m, idx) => {
              if (m.time > 0) {
                measurementData.data.push({
                  id: idx,
                  x: new Date(m.time * 1000).toLocaleString(),
                  y: m.weight,
                  selected: false,
                  rawData: m
                })
              }
            });
            let series = new Array<MeasurementData>(measurementData);
            setAllData(series);
          }
        }
      );
    } else {
      // filter data to update graph
      filterMeasurements();
    }
  }, [dataFilter, allData])

  return <>
    <div>
      <h1>Measurements</h1>
      {allData[0].data.length == 0 ? <LoadingImage></LoadingImage> : null}
      <div style={{ textAlign: "center" }}>
        <select value={dataFilter} onChange={updateMeasurementsFilter}>
          <option value={MeasurementFilter.LastMonth}>Last month</option>
          <option value={MeasurementFilter.AllData}>All measurements</option>
        </select>
      </div>
      <div style={{ height: "500px" }}>
        <ResponsiveLine
          enableSlices="x"
          data={filteredData}
          margin={{ top: 10, right: 5, bottom: 150, left: 60 }}
          xScale={{ type: 'point' }}
          yScale={{
            type: 'linear',
            min: 'auto',
            max: 'auto',
            // stacked: true,
            reverse: false
          }}
          yFormat=" >-.2f"
          axisBottom={{
            tickSize: 5,
            tickPadding: 5,
            tickRotation: -45,
            legend: 'date',
            legendOffset: 110,
            legendPosition: 'middle',
          }}
          axisLeft={{
            tickSize: 5,
            tickPadding: 5,
            tickRotation: 0,
            legend: 'cat weight in gram',
            legendOffset: -50,
            legendPosition: 'middle'
          }}
          axisRight={null}
          pointSize={10}
          pointBorderWidth={2}
          // enableArea={true}
          useMesh={true}
          layers={[
            StdDevLine,
            SelectedMeasurements,
            'grid',
            'markers',
            'areas',
            'crosshair',
            'lines',
            'slices',
            'axes',
            'points',
            'legends',
          ]}
          theme={{
            crosshair: {
              line: {
                stroke: '#774dd7',
                strokeOpacity: 1,
                strokeWidth: 2
              }
            }
          }}
        />
      </div>
    </div>
    <div>
      <table className="hoverable">
        <caption>Measurements</caption>
        <thead>
          <tr>
            <th>Date</th>
            <th>Weight (g)</th>
            <th>Duration (s)</th>
          </tr>
        </thead>
        <tbody>
          {filteredData[0].data.map((row, idx) => (
            <tr key={row.id} onClick={e => selectPoint(e, row.id)} className={(row.selected ? 'selected' : '')}>
              <td data-label="Date" className={(row.selected ? 'primary' : '')}>{row.x}</td>
              <td data-label="Weight">{row.y}</td>
              <td data-label="Duration">{row.rawData?.duration}</td>
            </tr>
          ))}
        </tbody>
      </table>
      <button onClick={deleteSelectedPoints}>Delete selected ({allData[0].data.filter(item => item.selected).length})</button>
    </div>
    <div>

      <a href="/api/measurements" className="button">Download as CSV</a>
      <form action="/api/measurements" method="POST" encType="multipart/form-data">
        <input name="measurements" type="file" className="icon-upload" />
        <input type="submit" value="Upload CSV (will overwrite data)" />
      </form>
    </div>
  </>
}

export default MeasurementHistory;