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
  const [dataHistory, setDataHistory] = useState<Array<Serie>>([new MeasurementData()]);
  var commonConfig = { delimiter: ",", dynamicTyping: true };

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
            measurementData.data.push({
              x: new Date(m.time * 1000).toISOString(),
              y: m.weight
            })
          });
          let series = new Array<Serie>(measurementData);
          setDataHistory(series);
        }
      }
    );
  }, [])

  return <>
    <div>
      <h1>Measurement history</h1>
      {dataHistory.length == 0 ? <img src={logo} className="App-logo" alt="logo" /> : null}
      {/* {lastMessage ? <span>Last message: {lastMessage.data}</span> : null} */}
      <div style={{ height: "500px" }}>
        <ResponsiveLine
          data={dataHistory}
          margin={{ top: 50, right: 110, bottom: 50, left: 60 }}
          xScale={{ type: 'point' }}
          yScale={{
            type: 'linear',
            min: 'auto',
            max: 'auto',
            stacked: true,
            reverse: false
          }}
          yFormat=" >-.2f"
          axisTop={null}
          axisRight={null}
          axisBottom={{
            tickSize: 5,
            tickPadding: 5,
            tickRotation: 0,
            legend: 'date',
            legendOffset: 36,
            legendPosition: 'middle'
          }}
          axisLeft={{
            tickSize: 5,
            tickPadding: 5,
            tickRotation: 0,
            legend: 'cat weight in gram',
            legendOffset: -50,
            legendPosition: 'middle'
          }}
          pointSize={10}
          pointColor={{ theme: 'background' }}
          pointBorderWidth={2}
          pointBorderColor={{ from: 'serieColor' }}
          pointLabelYOffset={-12}
          useMesh={true}
          // legends={[
          //   {
          //     anchor: 'bottom-right',
          //     direction: 'column',
          //     justify: false,
          //     translateX: 100,
          //     translateY: 0,
          //     itemsSpacing: 0,
          //     itemDirection: 'left-to-right',
          //     itemWidth: 80,
          //     itemHeight: 20,
          //     itemOpacity: 0.75,
          //     symbolSize: 12,
          //     symbolShape: 'circle',
          //     symbolBorderColor: 'rgba(0, 0, 0, .5)',
          //     effects: [
          //       {
          //         on: 'hover',
          //         style: {
          //           itemBackground: 'rgba(0, 0, 0, .03)',
          //           itemOpacity: 1
          //         }
          //       }
          //     ]
          //   }
          // ]}
        /></div>
    </div>
  </>
}

export default MeasurementHistory;