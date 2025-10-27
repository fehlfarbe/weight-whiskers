import React, { ChangeEvent, useEffect, useState } from "react";
import { Datum, ResponsiveLine, Serie } from '@nivo/line'
import { ResponsiveBar } from '@nivo/bar'
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
  dropping: number;
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

type HistogramDatum = {
  hour: number,
  value: number
}

const CreateEmptyHistogram = () => {
  let histogram: HistogramDatum[] = [];
  for (var i = 0; i < 24; i++) {
    histogram.push({ hour: i, value: 0 });
  }
  return histogram;
}


enum MeasurementFilter {
  LastMonth = "Last month",
  LastThreeMonths = "Last three months",
  AllData = "All data"
}


const MeasurementHistory = () => {
  const [dataFilter, setDataFilter] = useState<MeasurementFilter>(MeasurementFilter.LastMonth);
  const [filteredData, setFilteredData] = useState<Array<MeasurementData>>([new MeasurementData()]);
  const [histogramData, setHistogramData] = useState<Array<HistogramDatum>>(CreateEmptyHistogram());
  const [allData, setAllData] = useState<Array<MeasurementData>>([new MeasurementData()]);
  const commonConfig = { delimiter: ",", dynamicTyping: true };

  // update measurements filter via dropdown
  const updateMeasurementsFilter = (event: ChangeEvent<HTMLSelectElement>) => {
    // setFilteredData(dataHistory);
    setDataFilter(event.target.value as MeasurementFilter);
  }

  // filter data
  const filterMeasurements = () => {
    var measurements = new MeasurementData();
    if (dataFilter == MeasurementFilter.AllData) {
      measurements = allData[0];
    } else {
      var startDate = new Date();
      switch (dataFilter) {
        case MeasurementFilter.LastMonth:
          startDate.setMonth(startDate.getMonth() - 1);
          break;
        case MeasurementFilter.LastThreeMonths:
          startDate.setMonth(startDate.getMonth() - 3);
          break;
      }
      startDate.setHours(0);
      startDate.setMinutes(0);
      startDate.setSeconds(0);
      measurements.data = allData[0].data.filter(d => {
        if (d.rawData == undefined) {
          return false;
        }
        // compare timestamps to prevent different data formats
        return d.rawData?.time > startDate.getTime() / 1000.;
      });
      measurements.id = allData[0].id;
    }
    // calculate histogram
    let newHistogram = CreateEmptyHistogram();
    measurements.data.forEach(element => {
      if (element.rawData?.time) {
        let date = new Date(element.rawData?.time * 1000);
        newHistogram[date.getHours()].value++;
      }
    });
    // update data
    setFilteredData([measurements]);
    setHistogramData(newHistogram);
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

  const MovingAverageLine = (props: any) => {
    const { series, xScale, yScale } = props;

    // Dynamic window size depending on the data length
    // Min 5, max 15% of data points
    const calculateWindowSize = (dataLength: number): number => {
      const minWindow = 5;
      const maxWindow = Math.ceil(dataLength * 0.05);
      // odd number for symmetric window
      return Math.max(maxWindow, minWindow) | 1;
    };

    const windowSize = calculateWindowSize(series[0].data.length);
    const movingAverage = series[0].data.map((point: any, index: number, array: any[]) => {
      const start = Math.max(0, index - Math.floor(windowSize / 2));
      const end = Math.min(array.length, index + Math.floor(windowSize / 2) + 1);
      const window = array.slice(start, end);
      const sum = window.reduce((acc: number, curr: any) => acc + curr.data.y, 0);
      return {
        x: point.data.x,
        y: sum / window.length
      };
    });

    const linePath = `M ${movingAverage.map((p: any) =>
      `${xScale(p.x)},${yScale(p.y)}`).join(' L ')}`;

    return (
      <>
        {/* Äußere Linie für bessere Sichtbarkeit */}
        <path
          d={linePath}
          fill="none"
          stroke="white"
          strokeWidth={6}
          strokeLinecap="round"
          strokeOpacity={0.8}
        />
        {/* Innere Linie */}
        <path
          d={linePath}
          fill="none"
          stroke="#ff0000"
          strokeWidth={4}
          strokeLinecap="round"
        />
      </>
    );
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
        <div>Show data for</div>
        <select value={dataFilter} onChange={updateMeasurementsFilter}>
          {
            Object.values(MeasurementFilter).map(value => (
              <option key={value} value={value}>{value}</option>
            ))
          }
        </select>
      </div>
      <h2>Measured weight</h2>
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
            'grid',
            'markers',
            'areas',
            StdDevLine,
            'lines',
            SelectedMeasurements,
            'crosshair',
            'axes',
            'points',
            MovingAverageLine, // Moved to end for visibility
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
      <h2>Cat litter box usage by hour</h2>
      <div style={{ height: "500px" }}>
        <ResponsiveBar
          data={histogramData}
          keys={[
            'value',
          ]}
          indexBy="hour"
          margin={{ top: 50, right: 0, bottom: 50, left: 60 }}
          padding={0.3}
          valueScale={{ type: 'linear' }}
          indexScale={{ type: 'band', round: true }}
          colors={{ scheme: 'nivo' }}
          defs={[
            {
              id: 'dots',
              type: 'patternDots',
              background: 'inherit',
              color: '#38bcb2',
              size: 4,
              padding: 1,
              stagger: true
            },
            {
              id: 'lines',
              type: 'patternLines',
              background: 'inherit',
              color: '#eed312',
              rotation: -45,
              lineWidth: 6,
              spacing: 10
            }
          ]}
          borderColor={{
            from: 'color',
            modifiers: [
              [
                'darker',
                1.6
              ]
            ]
          }}
          axisTop={null}
          axisRight={null}
          axisBottom={{
            tickSize: 5,
            tickPadding: 5,
            tickRotation: 0,
            legend: 'Hour of day',
            legendPosition: 'middle',
            legendOffset: 32,
            truncateTickAt: 0
          }}
          axisLeft={{
            tickSize: 5,
            tickPadding: 5,
            tickRotation: 0,
            legend: 'Number of pees',
            legendPosition: 'middle',
            legendOffset: -40,
            truncateTickAt: 0
          }}
          labelSkipWidth={12}
          labelSkipHeight={12}
          labelTextColor={{
            from: 'color',
            modifiers: [
              [
                'darker',
                1.6
              ]
            ]
          }}
          role="application"
          ariaLabel="Cat litter box usage by hour"
          barAriaLabel={e => e.id + ": " + e.formattedValue + " on hour: " + e.indexValue}
        />
      </div>
    </div>
    <div>
      <table className="hoverable" style={{ overflowX: "hidden" }}>
        <caption>Measurements table</caption>
        <thead>
          <tr>
            <th>Date</th>
            <th>Weight (g)</th>
            <th>Duration (s)</th>
            <th>Weight of droppings (g)</th>
          </tr>
        </thead>
        <tbody>
          {filteredData[0].data.map((row, idx) => (
            <tr key={row.id} onClick={e => selectPoint(e, row.id)} className={(row.selected ? 'selected' : '')}>
              <td data-label="Date" className={(row.selected ? 'primary' : '')}>{row.x}</td>
              <td data-label="Weight">{row.y}</td>
              <td data-label="Duration">{row.rawData?.duration}</td>
              <td data-label="Weight of droppings">{row.rawData?.dropping}</td>
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