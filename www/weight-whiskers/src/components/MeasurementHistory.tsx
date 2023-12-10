import React, { ChangeEvent, MouseEventHandler, useEffect, useState } from "react";
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
}

class Point implements Datum {
  id: number = 0;
  x: string = "";
  y: number = 0;
  selected: boolean = false;
  rawData: MeasurementCSV|undefined = undefined;
}

class MeasurementData implements Serie {
  id: string = 'series';
  data: Array<Point> = [];
}

const MeasurementHistory = () => {
  const [dataHistory, setDataHistory] = useState<Array<MeasurementData>>([new MeasurementData()]);
  const commonConfig = { delimiter: ",", dynamicTyping: true };

  // on click table element
  const selectPoint = (event: React.MouseEvent<HTMLTableRowElement>, id: number) => {
    event.preventDefault();
    var history = dataHistory[0];
    var idx = history.data.findIndex(item => item.id == id);
    history.data[idx].selected = !history.data[idx].selected;
    setDataHistory([
      { ...history, data: history.data }
    ]
    );
    console.log(dataHistory);
  }

  // send delete request to delete selected elements identified by timestamp
  const deleteSelectedPoints = (event: React.MouseEvent<HTMLButtonElement>) => {
    event.preventDefault();
    var pointsToDelete = dataHistory[0].data.filter(d => d.selected);
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
          setDataHistory([
            {
              ...dataHistory[0],
              data: dataHistory[0].data.filter(d => !d.selected)
            }
          ]
          );
        }
      })
      .catch((error) => {
        console.log("Error");
      });
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
          console.log(result);
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
              <th>Date</th>
              <th>Weight (g)</th>
              <th>Duration (s)</th>
            </tr>
          </thead>
          <tbody>
            {dataHistory[0].data.map((row, idx) => (
              <tr key={row.id} onClick={e => selectPoint(e, row.id)} className={(row.selected ? 'selected' : '')}>
                <td data-label="Date" className={(row.selected ? 'primary' : '')}>{row.x}</td>
                <td data-label="Weight">{row.y}</td>
                <td data-label="Duration">{row.rawData?.duration}</td>
              </tr>
            ))}
          </tbody>
        </table>
        <button onClick={deleteSelectedPoints}>Delete selected ({dataHistory[0].data.filter(item => item.selected).length})</button>
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