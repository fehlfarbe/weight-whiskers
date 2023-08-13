import React, { useEffect, useState } from "react";
import { Datum, ResponsiveLine, Serie } from '@nivo/line'
import logo from '../logo.svg';
import Papa from "papaparse";

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
  x: string = "";
  y: number = 0;
}

class MeasurementData implements Serie {
  id: string = 'series';
  data: Array<Point> = [];
}

const MeasurementHistory = () => {
  const [dataHistory, setDataHistory] = useState<Array<MeasurementData>>([new MeasurementData()]);
  const commonConfig = { delimiter: ",", dynamicTyping: true };

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
          result.data.forEach(m => {
            if (m.time > 0) {
              measurementData.data.push({
                x: new Date(m.time * 1000).toLocaleString(),
                y: m.weight
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
      {dataHistory[0].data.length == 0 ? <img src={logo} className="App-loading" alt="loading" /> : null}
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
        <table>
          <caption>Measurements</caption>
          <thead>
            <tr>
              <th>Date</th>
              <th>Weight (g)</th>
            </tr>
          </thead>
          <tbody>
            {dataHistory[0].data.map((message, idx) => (
              <tr>
                <td data-label="Date">{message ? message.x : null}</td>
                <td data-label="Weight">{message ? message.y : null}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <div>

        <a href="/api/measurements" className="button">Download as CSV</a>
        <form action="/api/measurements" method="POST">
          <input name="measurements" type="file" className="icon-upload" />
          <input type="submit" value="Upload CSV" />
        </form>
      </div>
    </div>
  </>
}

export default MeasurementHistory;