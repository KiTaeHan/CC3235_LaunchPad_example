package main

import (
	"net/http"
	"github.com/gin-gonic/gin"
)

func main() {
	server := gin.Default()

	server.GET("/", func(ctx *gin.Context) {
		ctx.JSON(http.StatusOK, gin.H{"message": "Hello World"})
	})

	server.POST("/", func(ctx *gin.Context) {
		var request struct {
			Message string `json:"message"`
		}

		if err := ctx.ShouldBindJSON(&request); err != nil {
			ctx.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		}

		response := struct {
			Status  string `json:"status"`
			Message string `json:"message"`
		}{
			Status:  "success",
			Message: "Received message: " + request.Message,
		}

		ctx.JSON(http.StatusOK, response)
	})

	server.Run(":8080")
}
