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
  variance: number;
  sigma: number;
}

class Point implements Datum {
  id: number = 0;
  x: string = "";
  y: number = 0;
  selected: boolean = false;
}

class MeasurementData implements Serie {
  id: string = 'series';
  data: Array<Point> = [];
}

const MeasurementHistory = () => {
  const [dataHistory, setDataHistory] = useState<Array<MeasurementData>>([new MeasurementData()]);
  const commonConfig = { delimiter: ",", dynamicTyping: true };

  // on check table element
  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    console.log("onChange ", event.target.value);
    dataHistory[0].data[parseInt(event.target.value)].selected = !dataHistory[0].data[parseInt(event.target.value)].selected;
    setDataHistory(dataHistory);
    console.log(dataHistory[0].data[parseInt(event.target.value)].selected);
  }

  // load config
  useEffect(() => {
    Papa.parse(
      "/api/measurements",
      {
        ...commonConfig,
        header: true,
        download: true,
        complete: (result: Papa.ParseResult<MeasurementCSV>) => {
          let measurementData = new MeasurementData();
          measurementData.id = "Measured weight";
          result.data.forEach((m, idx) => {
            if (m.time > 0) {
              measurementData.data.push({
                id: idx,
                x: new Date(m.time * 1000).toLocaleString(),
                y: m.weight,
                selected: false
              })
            }
          });
          let series = new Array<MeasurementData>(measurementData);
          setDataHistory(series);
        }
      }
    );
  }, [])

  return <>
    <div>
      <h1>Measurements</h1>
      {dataHistory[0].data.length == 0 ? <LoadingImage></LoadingImage> : null}
      <div style={{ height: "500px" }}>
        <ResponsiveLine
          data={dataHistory}
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
          pointBorderColor={{ from: 'serieColor' }}
        // pointLabelYOffset={-12}
        useMesh={true}
        /></div>
      <div>
        <table className="hoverable">
          <caption>Measurements</caption>
          <thead>
            <tr>
              <th style={{width: "20px"}}></th>
              <th>Date</th>
              <th>Weight (g)</th>
            </tr>
          </thead>
          <tbody>
            {dataHistory[0].data.map((row, idx) => (
              <tr key={row.id}>
                <td data-label="Select"><input type="checkbox" value={row.id} checked={row.selected} onChange={handleChange}/></td>
                <td data-label="Date">{row.x}</td>
                <td data-label="Weight">{row.y}</td>
              </tr>
            ))}
          </tbody>
        </table>
        <button>Delete selected</button>
      </div>
      <div>

        <a href="/api/measurements" className="button">Download as CSV</a>
        <form action="/api/measurements" method="POST" encType="multipart/form-data">
          <input name="measurements" type="file" className="icon-upload" />
          <input type="submit" value="Upload CSV (will overwrite data)" />
        </form>
      </div>
    </div>
  </>
}

export default MeasurementHistory;