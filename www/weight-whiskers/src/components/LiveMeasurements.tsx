import React, { useEffect, useState } from "react";
import useWebSocket, { ReadyState } from 'react-use-websocket';
import { Datum, ResponsiveLine, Serie } from '@nivo/line'
import logo from '../logo.svg';

// @todo: set relative path
const WS_URL = 'ws://weight-whiskers.local/ws';


interface Measurement {
  timestamp: number;
  weight: number;
}

class Point implements Datum {
  x: number = 0;
  y: number = 0;
}

class MeasurementData implements Serie {
  id: string = 'series';
  data: Array<Point> = [];
  startTime: number = 0;
}

const LiveMeasurements = () => {
  const [dataHistory, setDataHistory] = useState<Array<MeasurementData>>([new MeasurementData()]);
  const { sendMessage, lastMessage, readyState } = useWebSocket(WS_URL, {
    onOpen: () => {
      console.log('opened');
    },
    share: true
  });

  useEffect(() => {
    if (lastMessage !== null) {
      let data = JSON.parse(lastMessage.data);
      if (dataHistory[0].startTime <= 0) {
        dataHistory[0].startTime = data.timestamp;
      }
      let p = new Point();
      p.x = (data.timestamp - dataHistory[0].startTime) / 1000.;
      p.y = data.weight;
      dataHistory[0].data.push(p);
      // delete old values (ringbuffer)
      if (dataHistory[0].data.length > 100) {
        dataHistory[0].data.shift();
      }
      setDataHistory(dataHistory);
    }
  }, [lastMessage, dataHistory, setDataHistory]);

  return <>
    <div>
      <h1>Live data</h1>
      {readyState != ReadyState.OPEN
        ? <img src={logo} className="App-loading" alt="loading" />
        : <div style={{ height: "500px" }}>
          <ResponsiveLine
            data={dataHistory}
            margin={{ top: 5, right: 5, bottom: 60, left: 60 }}
            yScale={{
              type: 'linear',
              min: 'auto',
              max: 'auto',
              stacked: true,
              reverse: false
            }}
            yFormat=" >-d"
            xFormat=".2f"
            axisBottom={{
              tickSize: 5,
              tickPadding: 5,
              tickRotation: -45,
              legend: 'time in seconds since start',
              legendOffset: 50,
              legendPosition: 'middle',
              format: '.2f',
            }}
            axisLeft={{
              tickSize: 5,
              tickPadding: 5,
              tickRotation: 0,
              legend: 'weight in gram',
              legendOffset: -50,
              legendPosition: 'middle',
              format: '.2f'
            }}
            enablePoints={false}
          /></div>
      }
    </div>
  </>
}

export default LiveMeasurements;