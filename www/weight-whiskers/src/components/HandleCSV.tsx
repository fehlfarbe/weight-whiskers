const HandleCSV = () => {
    return <div>
        <a href="/api/measurements" className="button">Download measurements as CSV</a>
        <form action="/api/measurements" method="POST" encType="multipart/form-data">
            <input name="measurements" type="file" className="icon-upload" />
            <input type="submit" value="Upload CSV (will overwrite data)" />
        </form>
    </div>
}

export default HandleCSV;