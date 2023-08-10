import React, { useEffect, useState, useRef, useCallback } from "react";
import useWebSocket, { ReadyState } from 'react-use-websocket';
import { Datum, ResponsiveLine, Serie } from '@nivo/line'
import { RingBuffer } from 'ring-buffer-ts';

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
}

const LiveMeasurements = () => {
  const [dataHistory, setDataHistory] = useState<Array<Serie>>([new MeasurementData()]);
  const { sendMessage, lastMessage, readyState } = useWebSocket(WS_URL, {
    onOpen: () => console.log('opened'),
    share: true
  });

  useEffect(() => {
    if (lastMessage !== null) {
      console.log("lastMessage", lastMessage.data);
      console.log("dataHistory length: ", dataHistory.length);
      console.log("dataHistory series length: ", dataHistory[0].data.length);
      let data = JSON.parse(lastMessage.data);
      let p = new Point();
      p.x = data.timestamp;
      p.y = data.weight;
      dataHistory[0].data.push(p);
      if(dataHistory[0].data.length > 100){
        dataHistory[0].data.shift();
      }
      setDataHistory(dataHistory);
      // dataHistory.data.push({x: data.timestamp, y: data.weight})
      // setDataHistory(prev => prev.concat(data));
    }
  }, [lastMessage, setDataHistory]);

  const handleClickSendMessage = useCallback(() => sendMessage('Hello'), []);

  const connectionStatus = {
    [ReadyState.CONNECTING]: 'Connecting',
    [ReadyState.OPEN]: 'Open',
    [ReadyState.CLOSING]: 'Closing',
    [ReadyState.CLOSED]: 'Closed',
    [ReadyState.UNINSTANTIATED]: 'Uninstantiated',
  }[readyState];


  return <>
    <div>
      <h1>Measurements:</h1>
      <span>The WebSocket is currently {connectionStatus}</span>
      {lastMessage ? <span>Last message: {lastMessage.data}</span> : null}
      <div style={{height: "500px"}}>
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
            legend: 'transportation',
            legendOffset: 36,
            legendPosition: 'middle'
          }}
          axisLeft={{
            tickSize: 5,
            tickPadding: 5,
            tickRotation: 0,
            legend: 'weight in gram',
            legendOffset: -40,
            legendPosition: 'middle'
          }}
          pointSize={10}
          pointColor={{ theme: 'background' }}
          pointBorderWidth={2}
          pointBorderColor={{ from: 'serieColor' }}
          pointLabelYOffset={-12}
          useMesh={true}
          legends={[
            {
              anchor: 'bottom-right',
              direction: 'column',
              justify: false,
              translateX: 100,
              translateY: 0,
              itemsSpacing: 0,
              itemDirection: 'left-to-right',
              itemWidth: 80,
              itemHeight: 20,
              itemOpacity: 0.75,
              symbolSize: 12,
              symbolShape: 'circle',
              symbolBorderColor: 'rgba(0, 0, 0, .5)',
              effects: [
                {
                  on: 'hover',
                  style: {
                    itemBackground: 'rgba(0, 0, 0, .03)',
                    itemOpacity: 1
                  }
                }
              ]
            }
          ]}
        /></div>
      <ul>
        {/* {dataHistory.map((message, idx) => (
          // <li key={idx}>{message ? message.timestamp : null} : {message ? message.weight : null}</li>
        ))} */}
      </ul>
    </div>
  </>
}

export default LiveMeasurements;