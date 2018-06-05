const configJson = require("./config.json");
const AWS = require("aws-sdk");
module.exports.compileSolidityToIele = function(event, context, callback) {
  AWS.config.update({
    accessKeyId: configJson["accessKeyId"],
    secretAccessKey: configJson["secretAccessKey"],
  });

  console.log("@updateConfig: ", configJson["accessKeyId"], configJson["secretAccessKey"])
  
  const ecs = new AWS.ECS();
  const params = {
    cluster: "sol2iele-cluster",
    taskDefinition: "compile-solidity-to-iele",
    overrides: {
      containerOverrides: [{
        name: "sol2iele-container",
        command: ["ls"]
      }]
    }
  };
  ecs.runTask(params, (error, data)=> {
    if (error) {
      return callback(error, null);
    } else {
      const response = {
        statusCode: 200,
        headers: {
          "Access-Control-Allow-Origin": "*", // Required for CORS support to work
          "Access-Control-Allow-Credentials": true // Required for cookies, authorization headers with HTTPS
        },
        body: JSON.stringify({
          success: true,
          data: JSON.stringify(data.tasks)
        })
      };
      console.log('@task: ', JSON.stringify(data.tasks));
      return callback(null, response);
    }
  });
}